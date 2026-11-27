// EXPECT: valid
// RUN: grid=1 local=1
// A marker after a break in the same case is unreachable.
kernel void fallthrough_after_break_in_case_accepted(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 0: x = 1u; break; [[fallthrough]]; x = 3u;
    case 1: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
