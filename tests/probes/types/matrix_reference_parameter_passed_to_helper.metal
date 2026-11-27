// EXPECT: valid
float4 apply(float4x4 m, float4 v) { return m * v; }
kernel void matrix_reference_parameter_passed_to_helper(device float4 *o [[buffer(0)]], constant float4x4 &m [[buffer(1)]]) {
  o[0] = apply(m, float4(0.0, 1.0, 0.0, 0.0));
}
