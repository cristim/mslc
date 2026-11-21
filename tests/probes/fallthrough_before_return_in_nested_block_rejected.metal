// EXPECT: error fallthrough annotation does not directly precede switch label
// A reachable marker followed by a statement still fails.
kernel void fallthrough_before_return_in_nested_block_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 0: if (y > 0u) { [[fallthrough]]; return; }
    case 1: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
