// EXPECT: error fallthrough annotation does not directly precede switch label
// A marker in the else branch inside a loop body.
kernel void fallthrough_in_else_of_if_in_for_body_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 0: for (uint j = 0u; j < 2u; j++) { if (j == 0u) { } else { [[fallthrough]]; } }
    case 9: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
