// EXPECT: valid
kernel void matrix_reference_parameter_store(device float2x2 &m [[buffer(0)]], device float4x4 &n [[buffer(1)]]) {
  m = float2x2(float2(5.0, 6.0), float2(7.0, 8.0));
  n[1] = float4(9.0);
}
