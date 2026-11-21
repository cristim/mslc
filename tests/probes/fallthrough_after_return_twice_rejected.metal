// EXPECT: error fallthrough annotation does not directly precede switch label
// Only the last marker of an unreachable run is ignored.
kernel void fallthrough_after_return_twice_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 0: return; [[fallthrough]]; [[fallthrough]];
    case 9: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
