// EXPECT: error fallthrough annotation does not directly precede switch label
// No outer label follows the inner switch.
kernel void fallthrough_ending_inner_switch_in_last_outer_case_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 9: x = 2u; break;
    case 0: switch (y) { case 1: { x = 1u; [[fallthrough]]; } }
  }
  o[0] = x;
}
