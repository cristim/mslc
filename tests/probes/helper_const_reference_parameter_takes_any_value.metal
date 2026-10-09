// EXPECT: valid
// A const reference cannot change its argument, so a buffer element and a
// temporary are both fine.
float len2(const float4 &v) { return dot(v, v); }
kernel void helper_const_reference_parameter_takes_any_value(device float *o [[buffer(0)]], constant float4 *c [[buffer(1)]]) {
  o[0] = len2(c[0]) + len2(float4(1.0));
}
