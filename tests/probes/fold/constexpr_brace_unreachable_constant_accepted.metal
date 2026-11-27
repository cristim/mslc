// EXPECT: valid
// A dead constexpr declaration made of constants is still fine, in a block and in a switch.
kernel void constexpr_brace_unreachable_constant_accepted(device int *out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  switch (gid) {
    case 0: out[0] = 1; break;
    constexpr int n{3};
  }
  return;
  constexpr int m{4};
}
