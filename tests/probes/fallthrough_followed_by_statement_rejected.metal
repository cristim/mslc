// EXPECT: error fallthrough annotation does not directly precede switch label
kernel void fallthrough_followed_by_statement_rejected(device int *o [[buffer(0)]], uint u [[thread_position_in_grid]]) {
  switch (u) { case 0: o[0] = 1; [[fallthrough]]; o[2] = 3; case 1: break; }
}
