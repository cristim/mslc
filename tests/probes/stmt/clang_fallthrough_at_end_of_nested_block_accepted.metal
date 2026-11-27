// EXPECT: valid
// RUN: grid=1 local=1
// Empty blocks and null statements after a nested marker do not count, as in Apple.
kernel void clang_fallthrough_at_end_of_nested_block_accepted(device uint *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint x = 0u;
  switch (i) {
    case 0: { { x = 1u; [[clang::fallthrough]]; } {} };
    case 1: x = 2u; break;
    default: break;
  }
  o[0] = x;
}
