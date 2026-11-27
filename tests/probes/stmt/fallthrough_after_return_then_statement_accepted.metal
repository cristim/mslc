// EXPECT: valid
// RUN: grid=1 local=1
// DIFFCHECK: writes-zero
// A statement after an unreachable marker is plain, so it is accepted.
kernel void fallthrough_after_return_then_statement_accepted(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 0: return; [[fallthrough]]; x = 3u;
    case 9: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
