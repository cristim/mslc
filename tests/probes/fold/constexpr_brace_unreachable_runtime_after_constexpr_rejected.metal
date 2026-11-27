// EXPECT: error a constexpr brace initializer requires a supported constant expression
// Naming an earlier constant does not make a runtime value one.
kernel void constexpr_brace_unreachable_runtime_after_constexpr_rejected(device int *out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  return;
  constexpr int a{1};
  constexpr int b{gid};
}
