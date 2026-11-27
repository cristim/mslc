// EXPECT: valid
// RUN: grid=1 local=1
// An if whose branches end in the marker reaches the label, as Apple accepts.
kernel void fallthrough_in_if_branches_accepted(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  switch (i) {
    case 0: if (x == 0u) { [[fallthrough]]; } else { [[fallthrough]]; }
    case 1: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
