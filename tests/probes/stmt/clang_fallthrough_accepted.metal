// EXPECT: valid
// Apple accepts [[clang::fallthrough]] wherever [[fallthrough]] is: before a label, in an empty case, nested at the end of a case.
kernel void clang_fallthrough_accepted(device int *o [[buffer(0)]], uint u [[thread_position_in_grid]]) {
  switch (u) {
    case 0: o[0] = 1; [[clang::fallthrough]];
    case 1: [[clang::fallthrough]];
    case 2: if (u) { [[clang::fallthrough]]; }
    default: o[1] = 2;
  }
}
