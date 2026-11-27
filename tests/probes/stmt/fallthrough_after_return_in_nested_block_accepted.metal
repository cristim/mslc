// EXPECT: valid
// RUN: grid=1 local=1
// A marker after a return cannot be reached and Apple ignores it, whatever follows.
kernel void fallthrough_after_return_in_nested_block_accepted(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 0: if (y > 0u) { return; [[fallthrough]]; x = 1u; }
    case 9: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
