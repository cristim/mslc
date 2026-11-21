// EXPECT: error fallthrough annotation does not directly precede switch label
// A statement follows the inner switch before the outer label.
kernel void fallthrough_ending_inner_switch_then_statement_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 0: switch (y) { case 1: { x = 1u; [[fallthrough]]; } }
    x = 5u;
    case 9: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
