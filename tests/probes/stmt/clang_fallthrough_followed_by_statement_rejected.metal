// EXPECT: error fallthrough annotation does not directly precede switch label
// Same placement rule as [[fallthrough]] (#151); Apple: "fallthrough annotation does not directly precede switch label".
kernel void k(device int *o [[buffer(0)]], uint u [[thread_position_in_grid]]) {
  switch (u) { case 0: o[0] = 1; [[clang::fallthrough]]; o[2] = 3; default: o[1] = 2; }
}
