# StemHub Backend

This backend depends on `PyFLP_enhanced` through a Git submodule, so StemHub does not vendor the full library history.

## Bootstrap (local + CI)

```bash
./backend/scripts/bootstrap-backend.sh
```

Equivalent commands:

```bash
git submodule sync --recursive
git submodule update --init --recursive
pip install -e backend/vendor/PyFLP_enhanced
pip install -e backend
```

## Verify dependency wiring

```bash
python -c "import pyflp; print(pyflp.__version__)"
```

## Contributing to `PyFLP_enhanced`

1. Work inside `backend/vendor/PyFLP_enhanced` on a branch.
2. Push your branch and open a PR in `aernw1/PyFLP_enhanced`.
3. After merge, update the submodule pointer in StemHub:

```bash
git submodule update --remote -- backend/vendor/PyFLP_enhanced
git add backend/vendor/PyFLP_enhanced .gitmodules
git commit -m "chore: bump PyFLP_enhanced submodule"
```

The StemHub workflow `sync-pyflp-submodule.yml` can also open this PR automatically when upstream pushes trigger repository dispatch.
