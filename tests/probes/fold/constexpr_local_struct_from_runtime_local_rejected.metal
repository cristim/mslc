// EXPECT: error constexpr variable "s" must be initialized by a constant expression
struct S { float value; };
kernel void constexpr_local_struct_from_runtime_local_rejected(device float *out [[buffer(0)]], device float *in [[buffer(1)]]) {
  S runtime;
  runtime.value = in[0];
  constexpr S s = runtime;
  out[0] = s.value;
}
