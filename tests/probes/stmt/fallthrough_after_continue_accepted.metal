// EXPECT: valid
// RUN: grid=1 local=1
// A marker after a continue cannot be reached and Apple ignores it.
kernel void fallthrough_after_continue_accepted(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  for (uint j = 0u; j < 2u; j++) {
  switch (i) {
    case 0: if (y > 0u) { continue; [[fallthrough]]; x = 1u; }
    case 9: x = 2u; break;
    default: break;
  }
  }
  o[0] = x;
}
