// EXPECT: error fallthrough annotation does not directly precede switch label
// A while body goes back to the loop head, not to the label.
kernel void fallthrough_in_while_body_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 0: while (x < 2u) { x += 1u; [[fallthrough]]; }
    case 1: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
