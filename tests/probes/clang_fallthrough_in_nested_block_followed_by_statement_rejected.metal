// EXPECT: error fallthrough annotation does not directly precede switch label
// The clang spelling is checked the same way (#355).
kernel void clang_fallthrough_in_nested_block_followed_by_statement_rejected(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  switch (i) {
    case 0: if (x == 0u) { [[clang::fallthrough]]; x = 1u; }
    case 1: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
