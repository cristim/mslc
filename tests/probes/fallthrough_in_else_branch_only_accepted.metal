// EXPECT: valid
// RUN: grid=1 local=1
// Only the else branch ends in the marker; the then branch reaches the label by falling off its end.
kernel void fallthrough_in_else_branch_only_accepted(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 0: if (y > 0u) { x = 4u; } else { [[fallthrough]]; }
    case 1: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
