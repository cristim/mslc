// EXPECT: valid
// A dead constexpr brace may name a constexpr declared before it in the dead region,
// in a block and in a switch, as it may where it is reached.
kernel void constexpr_brace_unreachable_names_earlier_constexpr(device int *out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  switch (gid) {
    case 0: out[0] = 1; break;
    constexpr int a{1};
    constexpr int b{a};
  }
  return;
  constexpr float x = 2;
  constexpr float4 v{x, x, x, x};
}
