// EXPECT: valid
// A float3x3 columns are 16 bytes apart, so a column of the reference is read at its own offset.
kernel void matrix_reference_parameter_column(device float *o [[buffer(0)]], constant float3x3 &m [[buffer(1)]]) {
  float3 a = m[0];
  float3 b = m[2];
  o[0] = a.x;
  o[1] = a.z;
  o[2] = b.x;
  o[3] = b.z;
}
