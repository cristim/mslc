// EXPECT: error a constexpr brace initializer requires a supported constant expression
kernel void constexpr_brace_unreachable_in_switch_rejected(device int *out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  switch (gid) {
    case 0: out[0] = 1; break;
    constexpr int n{gid};
  }
}
