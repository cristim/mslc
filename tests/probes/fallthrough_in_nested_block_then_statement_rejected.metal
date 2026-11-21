// EXPECT: error fallthrough annotation does not directly precede switch label
// The block ends in the marker but a statement follows the block before the label (#355).
kernel void fallthrough_in_nested_block_then_statement_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  switch (i) {
    case 0: { x = 1u; [[fallthrough]]; }
    x = 3u;
    case 1: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
