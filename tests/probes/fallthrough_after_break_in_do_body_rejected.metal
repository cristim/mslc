// EXPECT: error fallthrough annotation does not directly precede switch label
// A marker in a loop body is an error even after a break.
kernel void fallthrough_after_break_in_do_body_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 0: do { break; [[fallthrough]]; } while (true);
    case 9: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
