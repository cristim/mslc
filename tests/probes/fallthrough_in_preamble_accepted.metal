// EXPECT: valid
// RUN: grid=1 local=1
// DIFFCHECK: writes-zero
// Statements before the first label are not reached, so a marker among them is not checked.
kernel void fallthrough_in_preamble_accepted(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    x = 7u; [[fallthrough]]; x = 8u;
    case 1: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
