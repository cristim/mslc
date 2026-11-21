// EXPECT: error fallthrough annotation does not directly precede switch label
// A statement after an if whose else branch ends in the marker.
kernel void fallthrough_in_else_branch_then_statement_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 0: if (y > 0u) { x = 4u; } else { [[fallthrough]]; }
    x = 5u;
    case 1: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
