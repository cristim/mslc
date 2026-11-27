// EXPECT: error fallthrough annotation does not directly precede switch label
// The second marker is a statement after the first.
kernel void fallthrough_twice_in_a_row_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 0: x = 1u; [[fallthrough]]; [[fallthrough]];
    case 1: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
