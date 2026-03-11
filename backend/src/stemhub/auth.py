from fastapi import APIRouter, Depends, HTTPException, status, Request
from fastapi.security import OAuth2PasswordBearer, OAuth2PasswordRequestForm
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select
import jwt
import os
import httpx
import urllib.parse
import secrets
from fastapi.responses import RedirectResponse

from .database import get_db
from .models import User
from .schemas import LoginRequest, UserCreate, UserResponse, Token, UserUpdate, PasswordChangeRequest, EmailChangeRequest
from .security import get_password_hash, verify_password, create_access_token, SECRET_KEY, ALGORITHM

router = APIRouter(prefix="/auth", tags=["auth"])
oauth2_scheme = OAuth2PasswordBearer(tokenUrl="auth/swagger-login", auto_error=False)
INACTIVE_ACCOUNT_MESSAGE = "Your account is deactivated."

GOOGLE_CLIENT_ID = os.getenv("GOOGLE_CLIENT_ID")
GOOGLE_CLIENT_SECRET = os.getenv("GOOGLE_CLIENT_SECRET")

ENVIRONMENT = os.getenv("ENVIRONMENT", "local").lower()

def ensure_https(url: str) -> str:
    """Ensure HTTPS in non-local environments."""
    if ENVIRONMENT != "local" and url.startswith("http://"):
        return url.replace("http://", "https://", 1)
    return url

BACKEND_URL = ensure_https(os.getenv("BACKEND_URL", "http://localhost:8000"))
FRONTEND_URL = ensure_https(os.getenv("FRONTEND_URL", "http://localhost:3000"))
GOOGLE_REDIRECT_URI = ensure_https(os.getenv("GOOGLE_REDIRECT_URI", f"{BACKEND_URL}/auth/callback/google"))

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
        email = user_info.get("email")
        name = user_info.get("name")
        avatar_url = user_info.get("picture")

        if not email:
            raise HTTPException(status_code=400, detail="Failed to get email from Google")

        result = await db.execute(select(User).where(User.email == email))
        user = result.scalars().first()
        reactivated = False

        if not user:
            base_username = name.replace(" ", "").lower() if name else email.split("@")[0]
            username = base_username
            counter = 1
            while True:
                existing = await db.execute(select(User).where(User.username == username))
                if not existing.scalars().first():
                    break
                username = f"{base_username}{counter}"
                counter += 1

            password_hash = get_password_hash(urllib.parse.quote(email) + "oauth_dummy")
            user = User(email=email, username=username, password_hash=password_hash, avatar_url=avatar_url)
            db.add(user)
            reactivated = True
        else:
            user.avatar_url = avatar_url
            if not user.is_active:
                user.is_active = True
                reactivated = True

        if reactivated:
            await db.commit()
            await db.refresh(user)

        jwt_token = create_access_token(data={"sub": user.email})
        return RedirectResponse(url=f"{FRONTEND_URL}/auth/callback?token={jwt_token}")

@router.post("/register", response_model=UserResponse)
async def register(user: UserCreate, response: RedirectResponse, db: AsyncSession = Depends(get_db)):
    result_email = await db.execute(select(User).where(User.email == user.email))
    if result_email.scalars().first():
        raise HTTPException(status_code=400, detail="Email already registered")
        
    result_username = await db.execute(select(User).where(User.username == user.username))
    if result_username.scalars().first():
        raise HTTPException(status_code=400, detail="Username already taken")
    
    password_hash = get_password_hash(user.password)
    db_user = User(email=user.email, username=user.username, password_hash=password_hash)
    db.add(db_user)
    await db.commit()
    await db.refresh(db_user)
    
    # Set cookie on registration too
    access_token = create_access_token(data={"sub": db_user.email})
    response.set_cookie(
        key="access_token",
        value=access_token,
        httponly=True,
        max_age=3600 * 24 * 7,
        secure=ENVIRONMENT != "local",
        samesite="lax",
    )
    
    return db_user

@router.post("/swagger-login", response_model=Token)
async def swagger_login(response: RedirectResponse, form_data: OAuth2PasswordRequestForm = Depends(), db: AsyncSession = Depends(get_db)):
    # Swagger sends the user input in the "username" field, but we log in with email
    result = await db.execute(select(User).where(User.email == form_data.username))
    user = result.scalars().first()
    if user and not user.is_active:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail=INACTIVE_ACCOUNT_MESSAGE,
            headers={"WWW-Authenticate": "Bearer"},
        )
    if not user or not verify_password(form_data.password, user.password_hash):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Incorrect email or password",
            headers={"WWW-Authenticate": "Bearer"},
        )
    access_token = create_access_token(data={"sub": user.email})
    
    # Also set cookie for easier testing even in Swagger
    response.set_cookie(
        key="access_token",
        value=access_token,
        httponly=True,
        max_age=3600 * 24, # 24 hours
        secure=ENVIRONMENT != "local",
        samesite="lax",
    )
    return {"access_token": access_token, "token_type": "bearer"}

@router.post("/login", response_model=Token)
async def login(credentials: LoginRequest, response: RedirectResponse, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(User).where(User.email == credentials.email))
    user = result.scalars().first()
    if user and not user.is_active:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail=INACTIVE_ACCOUNT_MESSAGE,
            headers={"WWW-Authenticate": "Bearer"},
        )
    if not user or not verify_password(credentials.password, user.password_hash):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Incorrect email or password",
            headers={"WWW-Authenticate": "Bearer"},
        )
    
    access_token = create_access_token(data={"sub": user.email})
    
    # Set HttpOnly Cookie
    response.set_cookie(
        key="access_token",
        value=access_token,
        httponly=True,
        max_age=3600 * 24 * 7, # 7 days
        secure=ENVIRONMENT != "local",
        samesite="lax",
    )
    
    return {"access_token": access_token, "token_type": "bearer"}

@router.post("/logout")
async def logout(response: RedirectResponse):
    response.delete_cookie("access_token")
    return {"message": "Logged out successfully"}

async def get_current_user(request: Request, db: AsyncSession = Depends(get_db)):
    # Order of priority for tokens:
    # 1. Query parameter (explicitly passed, e.g. for audio)
    # 2. HttpOnly Cookie (automatic session)
    # 3. Authorization Header (Standard/Swagger)
    
    token = request.query_params.get("token")
    if not token:
        token = request.cookies.get("access_token")
    if not token:
        # Fallback to header if neither cookie nor query param exists
        header_auth = request.headers.get("Authorization")
        if header_auth and header_auth.startswith("Bearer "):
            token = header_auth.split(" ")[1]

    credentials_exception = HTTPException(
        status_code=status.HTTP_401_UNAUTHORIZED,
        detail="Could not validate credentials",
        headers={"WWW-Authenticate": "Bearer"},
    )
    if not token:
        raise credentials_exception

    try:
        payload = jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM])
        email = payload.get("sub")
        if not isinstance(email, str):
            raise credentials_exception
    except jwt.PyJWTError:
        raise credentials_exception
        
    result = await db.execute(select(User).where(User.email == email, User.is_active == True))
    user = result.scalars().first()
    if user is None:
        raise credentials_exception
    return user

@router.get("/me", response_model=UserResponse)
async def get_me(current_user: User = Depends(get_current_user)):
    return current_user

@router.put("/me", response_model=UserResponse)
async def update_me(user_update: UserUpdate, current_user: User = Depends(get_current_user), db: AsyncSession = Depends(get_db)):
    if user_update.username is not None:
        if user_update.username != current_user.username:
            result = await db.execute(select(User).where(User.username == user_update.username))
            if result.scalars().first():
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
    await db.commit()
    await db.refresh(current_user)
    return current_user

@router.put("/me/password")
async def update_password(password_data: PasswordChangeRequest, current_user: User = Depends(get_current_user), db: AsyncSession = Depends(get_db)):
    if not verify_password(password_data.current_password, current_user.password_hash):
        raise HTTPException(status_code=400, detail="L'ancien mot de passe est incorrect")
    
    current_user.password_hash = get_password_hash(password_data.new_password)
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


async def get_current_admin_user(current_user: User = Depends(get_current_user)) -> User:
    if not current_user.is_admin:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="Admin access required",
        )
    return current_user
