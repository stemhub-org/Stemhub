"""The PyFLP_v2 commit this backend is built and tested against."""

# Full commit of the backend/vendor/PyFLP_v2 submodule. Bump it in the same
# commit as the submodule pointer: tests/test_parser_version.py fails while
# the two differ. The backend image installs PyFLP from that submodule, so
# this is also the parser that runs in production.
PYFLP_COMMIT = "976c2f329672a1e59dc0efaf9ebe0b604571326c"
