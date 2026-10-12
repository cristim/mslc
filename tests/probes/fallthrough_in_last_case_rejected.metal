// EXPECT: error fallthrough annotation does not directly precede switch label
// Apple: "fallthrough annotation does not directly precede switch label".
kernel void fallthrough_in_last_case_rejected(device int *o [[buffer(0)]], uint u [[thread_position_in_grid]]) {
  switch (u) { case 0: o[0] = 1; break; default: o[1] = 2; [[fallthrough]]; }
}
