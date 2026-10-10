// EXPECT: error cannot store through
kernel void matrix_reference_parameter_store_through_constant_rejected(constant float4x4 &m [[buffer(0)]]) {
  m = float4x4(1.0);
}
