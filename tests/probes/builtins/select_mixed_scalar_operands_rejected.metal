// EXPECT: error the operands of select have to be one type
kernel void select_mixed_scalar_operands_rejected(device float4 *o [[buffer(0)]], device const float4 *in [[buffer(1)]]) {
  float4 a = in[0];
  o[0].x = select(1.0f, 2, true);
}
