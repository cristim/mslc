// EXPECT: valid
// round on a half vector and a float vector in one kernel.
kernel void math_round_vectors(device float4 *o [[buffer(0)]], constant float4 *f [[buffer(1)]], constant half3 *h [[buffer(2)]]) {
  o[0] = round(f[0]);
  o[1] = float4(float3(round(h[0])), 0.0);
}
