// EXPECT: error fallthrough annotation does not directly precede switch label
// A loop body goes back to the loop head, not on to the label (#355).
kernel void fallthrough_in_loop_body_before_label_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  switch (i) {
    case 0: for (uint j = 0u; j < 2u; j++) { x += j; [[fallthrough]]; }
    case 1: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
