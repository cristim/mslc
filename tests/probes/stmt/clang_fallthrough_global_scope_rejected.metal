// EXPECT: error unexpected [ "[" in an expression
// Apple rejects [[::clang::fallthrough]] ("expected ']'"); only the plain and clang:: spellings are accepted.
kernel void k(device int *o [[buffer(0)]], uint u [[thread_position_in_grid]]) {
  switch (u) { case 0: o[0] = 1; [[::clang::fallthrough]]; default: o[1] = 2; }
}
