// EXPECT: error fallthrough annotation does not directly precede switch label
// The marker is nested in a block and a statement follows it in that block (#355).
kernel void fallthrough_in_nested_block_followed_by_statement_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  switch (i) {
    case 0: { [[fallthrough]]; x = 1u; }
    case 1: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
