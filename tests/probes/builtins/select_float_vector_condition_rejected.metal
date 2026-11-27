// EXPECT: error the condition of select beside 4-component operands has to be a bool or a bool4
kernel void select_float_vector_condition_rejected(device float4 *o [[buffer(0)]], device const float4 *in [[buffer(1)]]) {
  float4 a = in[0];
  o[0] = select(a, in[1], in[2]);
}
