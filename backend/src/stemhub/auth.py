from fastapi import APIRouter, Depends, HTTPException, Request, Response, status
from fastapi.security import OAuth2PasswordBearer, OAuth2PasswordRequestForm
from sqlalchemy import func, update
from sqlalchemy.exc import IntegrityError
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select
import jwt
import os
import uuid
from datetime import datetime, timedelta, timezone
import httpx
import secrets
from fastapi.responses import RedirectResponse

from .database import get_db
from .login_throttle import ensure_login_allowed, record_login_attempt
from .pwned_passwords import BREACHED_PASSWORD_MESSAGE, is_password_pwned
from .rate_limit import enforce_rate_limit
from .models import User, UserIdentity, UserSession
from .schemas import USERNAME_MAX_LENGTH, USERNAME_MIN_LENGTH, normalize_email, normalize_username, LoginRequest, UserCreate, UserResponse, Token, UserUpdate, PasswordChangeRequest, EmailChangeRequest
from .security import ACCESS_TOKEN_EXPIRE_MINUTES, UNUSABLE_PASSWORD_HASH, get_password_hash, verify_password, create_access_token, SECRET_KEY, ALGORITHM

router = APIRouter(prefix="/auth", tags=["auth"])
oauth2_scheme = OAuth2PasswordBearer(tokenUrl="auth/swagger-login", auto_error=False)
INACTIVE_ACCOUNT_MESSAGE = "Your account is deactivated."

GOOGLE_CLIENT_ID = os.getenv("GOOGLE_CLIENT_ID")
GOOGLE_CLIENT_SECRET = os.getenv("GOOGLE_CLIENT_SECRET")

ENVIRONMENT = os.getenv("ENVIRONMENT", "local").lower()

async def is_username_taken(db: AsyncSession, username: str, *, exclude_user_id=None) -> bool:
    """Case-insensitive twin of the unique index uq_users_username_lower, so
    the client gets a 400 rather than a 500. The index remains the actual
    guarantee under concurrent requests (see `_commit_user`)."""
    query = select(User.id).where(func.lower(User.username) == func.lower(username))
    if exclude_user_id is not None:
        query = query.where(User.id != exclude_user_id)
    result = await db.execute(query)
    return result.first() is not None


async def _commit_user(db: AsyncSession) -> None:
    """Commit a new or updated user, turning a unique-index race into a 400."""
    try:
        await db.commit()
    except IntegrityError as exc:
        await db.rollback()
        if "username" in str(exc.orig):
            raise HTTPException(status_code=400, detail="Username already taken")
        raise HTTPException(status_code=400, detail="Email already registered")


def ensure_https(url: str) -> str:
    """Ensure HTTPS in non-local environments."""
    if ENVIRONMENT != "local" and url.startswith("http://"):
        return url.replace("http://", "https://", 1)
    return url

BACKEND_URL = ensure_https(os.getenv("BACKEND_URL", "http://localhost:8000"))
FRONTEND_URL = ensure_https(os.getenv("FRONTEND_URL", "http://localhost:3000"))
GOOGLE_REDIRECT_URI = ensure_https(os.getenv("GOOGLE_REDIRECT_URI", f"{BACKEND_URL}/auth/callback/google"))

SESSION_LIFETIME = timedelta(minutes=ACCESS_TOKEN_EXPIRE_MINUTES)
# "Remember me" at login.
REMEMBERED_SESSION_LIFETIME = timedelta(days=30)


async def _start_session(db: AsyncSession, user: User, *, remember: bool = False) -> str:
    """Record a server-side session for `user` and return its access token.
    The token is only accepted while the session lives (see get_current_user)."""
    expires_at = datetime.now(timezone.utc) + (REMEMBERED_SESSION_LIFETIME if remember else SESSION_LIFETIME)
    session = UserSession(id=uuid.uuid4(), user_id=user.id, expires_at=expires_at)
    db.add(session)
    await db.commit()
    return create_access_token(data={"sub": user.email, "sid": str(session.id)}, expires_at=expires_at)


def _set_session_cookie(response: Response, token: str, *, remember: bool = False) -> None:
    """A remembered session's cookie lasts as long as the session. Otherwise
    it is a browser-session cookie, gone when the browser closes."""
    response.set_cookie(
        key="access_token",
        value=token,
        httponly=True,
        max_age=int(REMEMBERED_SESSION_LIFETIME.total_seconds()) if remember else None,
        secure=ENVIRONMENT != "local",
        samesite="lax",
    )


def _request_token(request: Request) -> str | None:
    # Order of priority for tokens:
    # 1. Query parameter (explicitly passed, e.g. for audio)
    # 2. HttpOnly Cookie (sign-in session, sent automatically)
    # 3. Authorization Header (Standard/Swagger)
    token = request.query_params.get("token")
    if not token:
        token = request.cookies.get("access_token")
    if not token:
        header_auth = request.headers.get("Authorization")
        if header_auth and header_auth.startswith("Bearer "):
            token = header_auth.split(" ")[1]
    return token


def _session_id(token: str | None) -> uuid.UUID | None:
    """The session a valid, unexpired token belongs to."""
    if not token:
        return None
    try:
        payload = jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM])
        return uuid.UUID(str(payload.get("sid")))
    except (jwt.PyJWTError, ValueError):
        return None


def _ensure_can_sign_in(user: User) -> None:
    """Social sign-in must not bring back an account an admin deactivated
    (it used to reactivate it) or one that was deleted."""
    if user.is_deleted or not user.is_active:
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail=INACTIVE_ACCOUNT_MESSAGE)


@router.get("/login/google")
async def login_google():
    if not GOOGLE_CLIENT_ID:
        raise HTTPException(status_code=500, detail="Google OAuth is not configured (GOOGLE_CLIENT_ID missing).")
    state = secrets.token_urlsafe(32)
    response = RedirectResponse(
        url=f"https://accounts.google.com/o/oauth2/v2/auth?response_type=code&client_id={GOOGLE_CLIENT_ID}&redirect_uri={GOOGLE_REDIRECT_URI}&scope=openid%20profile%20email&access_type=offline&prompt=select_account&state={state}"
    )
    response.set_cookie(
        key="oauth_state",
        value=state,
        httponly=True,
        max_age=600,
        secure=ENVIRONMENT != "local",
        samesite="lax",
    )
    return response

@router.get("/callback/google")
async def callback_google(request: Request, code: str, state: str | None = None, db: AsyncSession = Depends(get_db)):
    if not GOOGLE_CLIENT_ID or not GOOGLE_CLIENT_SECRET:
        raise HTTPException(status_code=500, detail="Google OAuth is not configured (GOOGLE_CLIENT_ID/GOOGLE_CLIENT_SECRET missing).")
    cookie_state = request.cookies.get("oauth_state")
    if not state or not cookie_state or not secrets.compare_digest(state, cookie_state):
        raise HTTPException(status_code=400, detail="Invalid state parameter")

    token_url = "https://oauth2.googleapis.com/token"
    async with httpx.AsyncClient() as client:
        token_res = await client.post(token_url, data={
            "client_id": GOOGLE_CLIENT_ID,
            "client_secret": GOOGLE_CLIENT_SECRET,
            "code": code,
            "grant_type": "authorization_code",
            "redirect_uri": GOOGLE_REDIRECT_URI
        })
        token_data = token_res.json()
        if token_data.get("error"):
            error_code = token_data.get("error")
            error_description = token_data.get("error_description") or "OAuth token exchange failed."
            raise HTTPException(
                status_code=400,
                detail=f"{error_code}: {error_description}"
            )

        access_token = token_data.get("access_token")

        if not access_token:
            raise HTTPException(status_code=400, detail="Failed to get access token from Google")

        user_info_res = await client.get("https://www.googleapis.com/oauth2/v2/userinfo", headers={"Authorization": f"Bearer {access_token}"})
        user_info = user_info_res.json()
        google_id = user_info.get("id")
        email = normalize_email(user_info.get("email") or "")
        name = user_info.get("name")
        avatar_url = user_info.get("picture")

        if not google_id or not email:
            raise HTTPException(status_code=400, detail="Failed to get account details from Google")
        # A Google account seen for the first time is linked by email, so an
        # unverified Google address would let anyone claim the StemHub account
        # that uses it.
        if user_info.get("verified_email") is not True:
            raise HTTPException(status_code=400, detail="Your Google email address is not verified")

        result = await db.execute(
            select(User)
            .join(UserIdentity, UserIdentity.user_id == User.id)
            .where(UserIdentity.provider == "google", UserIdentity.provider_user_id == str(google_id))
        )
        user = result.scalars().first()
        needs_commit = False

        if not user:
            result = await db.execute(select(User).where(User.email == email))
            user = result.scalars().first()
            if user:
                _ensure_can_sign_in(user)
                db.add(UserIdentity(user_id=user.id, provider="google", provider_user_id=str(google_id), email=email))
                needs_commit = True

        if not user:
            base_username = normalize_username(name.replace(" ", "").lower() if name else email.split("@")[0])
            base_username = base_username[:USERNAME_MAX_LENGTH - 6].ljust(USERNAME_MIN_LENGTH, "0")
            username = base_username
            counter = 1
            while await is_username_taken(db, username):
                username = f"{base_username}{counter}"
                counter += 1

            user = User(email=email, username=username, password_hash=UNUSABLE_PASSWORD_HASH, avatar_url=avatar_url)
            user.identities.append(UserIdentity(provider="google", provider_user_id=str(google_id), email=email))
            db.add(user)
            needs_commit = True
        else:
            _ensure_can_sign_in(user)
            user.avatar_url = avatar_url

        if needs_commit:
            await _commit_user(db)
            await db.refresh(user)

        jwt_token = await _start_session(db, user)
        return RedirectResponse(url=f"{FRONTEND_URL}/auth/callback?token={jwt_token}")

REGISTER_LIMIT = 10
REGISTER_WINDOW = timedelta(hours=1)


@router.post("/register", response_model=UserResponse)
async def register(user: UserCreate, request: Request, response: RedirectResponse, db: AsyncSession = Depends(get_db)):
    # Every attempt counts, refused ones too, so a script can neither mass
    # create accounts nor probe which emails are registered.
    if request.client:
        await enforce_rate_limit(
            db,
            action="register",
            key=request.client.host,
            limit=REGISTER_LIMIT,
            window=REGISTER_WINDOW,
            message="Too many sign-ups from your network.",
        )
    if await is_password_pwned(user.password):
        raise HTTPException(status_code=400, detail=BREACHED_PASSWORD_MESSAGE)

    result_email = await db.execute(select(User).where(User.email == user.email))
    if result_email.scalars().first():
        raise HTTPException(status_code=400, detail="Email already registered")
        
    if await is_username_taken(db, user.username):
        raise HTTPException(status_code=400, detail="Username already taken")
    
    password_hash = get_password_hash(user.password)
    db_user = User(email=user.email, username=user.username, password_hash=password_hash)
    db.add(db_user)
    await _commit_user(db)
    await db.refresh(db_user)
    
    # Set cookie on registration too
    access_token = await _start_session(db, db_user)
    _set_session_cookie(response, access_token)
    
    return db_user

async def _authenticate(db: AsyncSession, email: str, password: str, request: Request) -> User:
    """The user these credentials belong to. Every failure gets the same 401
    and takes as long, so the response never reveals whether an email has an
    account. Only someone who knows the password learns the account is
    deactivated. Repeated failures are throttled (login_throttle.py)."""
    ip = request.client.host if request.client else None
    await ensure_login_allowed(db, email, ip)

    result = await db.execute(select(User).where(User.email == email, User.is_deleted == False))
    user = result.scalars().first()
    password_ok = verify_password(password, user.password_hash if user else None)
    record_login_attempt(db, email, ip, succeeded=password_ok)
    if not password_ok:
        await db.commit()
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Incorrect email or password",
            headers={"WWW-Authenticate": "Bearer"},
        )
    if not user.is_active:
        await db.commit()
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail=INACTIVE_ACCOUNT_MESSAGE,
            headers={"WWW-Authenticate": "Bearer"},
        )
    # The successful attempt is committed with the new session.
    return user

@router.post("/swagger-login", response_model=Token)
async def swagger_login(request: Request, response: RedirectResponse, form_data: OAuth2PasswordRequestForm = Depends(), db: AsyncSession = Depends(get_db)):
    # Swagger sends the user input in the "username" field, but we log in with email
    user = await _authenticate(db, normalize_email(form_data.username), form_data.password, request)
    access_token = await _start_session(db, user)
    
    # Also set cookie for easier testing even in Swagger
    _set_session_cookie(response, access_token)
    return {"access_token": access_token, "token_type": "bearer"}

@router.post("/login", response_model=Token)
async def login(credentials: LoginRequest, request: Request, response: RedirectResponse, db: AsyncSession = Depends(get_db)):
    user = await _authenticate(db, credentials.email, credentials.password, request)

    access_token = await _start_session(db, user, remember=credentials.remember_me)
    _set_session_cookie(response, access_token, remember=credentials.remember_me)
    
    return {"access_token": access_token, "token_type": "bearer"}

@router.post("/logout")
async def logout(request: Request, response: RedirectResponse, db: AsyncSession = Depends(get_db)):
    """Ends the session the request's token belongs to, so the token stops
    working everywhere, not only in this browser."""
    session_id = _session_id(_request_token(request))
    if session_id is not None:
        await db.execute(
            update(UserSession)
            .where(UserSession.id == session_id, UserSession.revoked_at.is_(None))
            .values(revoked_at=func.now())
        )
        await db.commit()
    response.delete_cookie("access_token")
    return {"message": "Logged out successfully"}

async def get_current_user(request: Request, db: AsyncSession = Depends(get_db)):
    credentials_exception = HTTPException(
        status_code=status.HTTP_401_UNAUTHORIZED,
        detail="Could not validate credentials",
        headers={"WWW-Authenticate": "Bearer"},
    )
    session_id = _session_id(_request_token(request))
    if session_id is None:
        raise credentials_exception

    result = await db.execute(
        select(User)
        .join(UserSession, UserSession.user_id == User.id)
        .where(
            UserSession.id == session_id,
            UserSession.revoked_at.is_(None),
            UserSession.expires_at > func.now(),
            User.is_active == True,
            User.is_deleted == False,
        )
    )
    user = result.scalars().first()
    if user is None:
        raise credentials_exception
    return user


async def get_current_admin_user(
    current_user: User = Depends(get_current_user),
) -> User:
    """Require the authenticated user to have admin privileges."""
    if not current_user.is_admin:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="Admin privileges required",
        )
    return current_user


@router.get("/me", response_model=UserResponse)
async def get_me(current_user: User = Depends(get_current_user)):
    return current_user

@router.put("/me", response_model=UserResponse)
async def update_me(user_update: UserUpdate, current_user: User = Depends(get_current_user), db: AsyncSession = Depends(get_db)):
    if user_update.username is not None:
        # Excluding the user's own row lets them change only the case of their name.
        if await is_username_taken(db, user_update.username, exclude_user_id=current_user.id):
            raise HTTPException(status_code=400, detail="Username already taken")
        current_user.username = user_update.username

    if user_update.avatar_url is not None:
        current_user.avatar_url = user_update.avatar_url

    if user_update.bio is not None:
        current_user.bio = user_update.bio
    
    if user_update.location is not None:
        current_user.location = user_update.location
    
    if user_update.website is not None:
        current_user.website = user_update.website
    if user_update.genres is not None:
        current_user.genres = user_update.genres
    await _commit_user(db)
    await db.refresh(current_user)
    return current_user

@router.put("/me/password")
async def update_password(password_data: PasswordChangeRequest, request: Request, current_user: User = Depends(get_current_user), db: AsyncSession = Depends(get_db)):
    if not verify_password(password_data.current_password, current_user.password_hash):
        raise HTTPException(status_code=400, detail="L'ancien mot de passe est incorrect")
    if await is_password_pwned(password_data.new_password):
        raise HTTPException(status_code=400, detail=BREACHED_PASSWORD_MESSAGE)

    current_user.password_hash = get_password_hash(password_data.new_password)
    # Sign out every other device: whoever knew the old password and signed
    # in with it loses access. The session making the change stays.
    await db.execute(
        update(UserSession)
        .where(
            UserSession.user_id == current_user.id,
            UserSession.id != _session_id(_request_token(request)),
            UserSession.revoked_at.is_(None),
        )
        .values(revoked_at=func.now())
    )
    await db.commit()
    return {"message": "Mot de passe mis à jour avec succès"}

@router.put("/me/email", response_model=UserResponse)
async def update_email(email_data: EmailChangeRequest, current_user: User = Depends(get_current_user), db: AsyncSession = Depends(get_db)):
    if email_data.new_email == current_user.email:
        raise HTTPException(status_code=400, detail="C'est déjà votre adresse email")

    result = await db.execute(select(User).where(User.email == email_data.new_email))
    if result.scalars().first():
        raise HTTPException(status_code=400, detail="Cet email est déjà pris")

    current_user.email = email_data.new_email
    await db.commit()
    await db.refresh(current_user)
    return current_user


