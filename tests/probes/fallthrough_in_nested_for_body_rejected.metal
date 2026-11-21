// EXPECT: error fallthrough annotation does not directly precede switch label
// A marker in a loop nested in a loop.
kernel void fallthrough_in_nested_for_body_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 0: for (uint j = 0u; j < 2u; j++) { for (uint k = 0u; k < 2u; k++) { [[fallthrough]]; } }
    case 9: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
