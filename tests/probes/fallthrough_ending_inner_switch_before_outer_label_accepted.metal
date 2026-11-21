// EXPECT: valid
// RUN: grid=1 local=1
// A marker ending the last case of a nested switch falls out to the outer label, as Apple accepts.
kernel void fallthrough_ending_inner_switch_before_outer_label_accepted(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 0: switch (y) { case 1: { x = 1u; [[fallthrough]]; } }
    case 9: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
