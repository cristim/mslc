// EXPECT: valid
float4 expand(bool v) { return v ? float4(1.0) : float4(0.0); }
kernel void bool_reference_parameter_passed_to_helper(device float4 *o [[buffer(0)]], constant bool &b [[buffer(1)]]) {
  o[0] = expand(b);
}
