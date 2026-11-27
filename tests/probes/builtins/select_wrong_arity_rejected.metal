// EXPECT: error select takes 3 arguments, and this call passes 2
kernel void select_wrong_arity_rejected(device float4 *o [[buffer(0)]], device const float4 *in [[buffer(1)]]) {
  float4 a = in[0];
  o[0] = select(a, in[1]);
}
