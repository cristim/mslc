// EXPECT: error fallthrough annotation does not directly precede switch label
// A marker ending an inner switch inside a loop body falls out into the loop.
kernel void fallthrough_in_switch_in_for_body_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 0: for (uint j = 0u; j < 2u; j++) { switch (j) { case 0: x += 1u; [[fallthrough]]; } }
    case 9: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
