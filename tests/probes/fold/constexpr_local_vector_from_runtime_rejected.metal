// EXPECT: error constexpr variable "v" must be initialized by a constant expression
kernel void constexpr_local_vector_from_runtime_rejected(device float *out [[buffer(0)]], device float *in [[buffer(1)]]) {
  constexpr float3 v = float3(in[0]);
  out[0] = v.x;
}
