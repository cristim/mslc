// EXPECT: error fallthrough annotation does not directly precede switch label
// No label follows the nested marker in the last case (#355).
kernel void fallthrough_in_nested_block_of_last_case_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  switch (i) {
    case 0: x = 1u; break;
    case 1: { x = 2u; [[fallthrough]]; }
  }
  o[0] = x;
}
