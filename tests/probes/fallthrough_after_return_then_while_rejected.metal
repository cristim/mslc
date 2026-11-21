// EXPECT: error fallthrough annotation does not directly precede switch label
// An unreachable marker must still reach the label through plain statements, as Apple requires.
kernel void fallthrough_after_return_then_while_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  uint y = i;
  switch (i) {
    case 0: return; [[fallthrough]]; while (y < 3u) { y++; }
    case 9: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
