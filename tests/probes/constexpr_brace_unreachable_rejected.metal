// EXPECT: error a constexpr brace initializer requires a supported constant expression
// Apple: "non-constant-expression cannot be narrowed from type 'uint' to 'int'".
kernel void constexpr_brace_unreachable_rejected(device int *out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  return;
  constexpr int n{gid};
}
