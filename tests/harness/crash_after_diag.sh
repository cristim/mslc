#!/bin/sh
# Fake compiler for the harness self-test: prints the expected
# diagnostic, then dies of SIGSEGV.
echo "error: expected diagnostic" >&2
kill -SEGV $$
