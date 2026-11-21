// EXPECT: error fallthrough annotation does not directly precede switch label
// Two markers in an unreachable run, the first in an else branch.
kernel void fallthrough_after_return_in_else_then_marker_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 0: return; if (y > 0u) { x = 1u; } else { [[fallthrough]]; } [[fallthrough]];
    case 9: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
