// EXPECT: error fallthrough annotation does not directly precede switch label
// A null statement is ignored, but a real one after it still separates marker and label.
kernel void fallthrough_null_then_statement_rejected(device int *o [[buffer(0)]], uint u [[thread_position_in_grid]]) {
  switch (u) { case 0: o[0] = 1; [[fallthrough]]; ; o[2] = 3; default: break; }
}
