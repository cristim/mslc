// EXPECT: valid
// Apple ignores a null statement between the marker and the label.
kernel void fallthrough_null_statement_accepted(device int *o [[buffer(0)]], uint u [[thread_position_in_grid]]) {
  switch (u) { case 0: o[0] = 1; [[fallthrough]]; ; ; case 1: o[1] = 2; break; default: break; }
}
