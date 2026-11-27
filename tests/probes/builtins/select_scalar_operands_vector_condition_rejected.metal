// EXPECT: error select with a vector condition takes vector operands
kernel void select_scalar_operands_vector_condition_rejected(device float4 *o [[buffer(0)]], device const float4 *in [[buffer(1)]]) {
  float4 a = in[0];
  o[0] = select(1.0f, 2.0f, bool4(true));
}
