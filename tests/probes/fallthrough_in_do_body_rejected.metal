// EXPECT: error fallthrough annotation does not directly precede switch label
// A do body goes on to the condition, not to the label.
kernel void fallthrough_in_do_body_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 0: do { x += 1u; [[fallthrough]]; } while (x < 2u);
    case 1: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
