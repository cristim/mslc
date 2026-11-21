// EXPECT: error fallthrough annotation does not directly precede switch label
// The same after a continue.
kernel void fallthrough_after_continue_in_for_body_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 0: for (uint j = 0u; j < 2u; j++) { continue; [[fallthrough]]; }
    case 9: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
