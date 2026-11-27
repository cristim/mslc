// EXPECT: error no matching conversion from a matrix of another shape
// Apple: "no matching conversion for functional-style cast from float3x3 to float2x2".
kernel void matrix_from_other_shape_rejected(device float *o [[buffer(0)]]) {
  float3x3 a = float3x3(1.0);
  float2x2 m = float2x2(a);
  o[0] = m[0][0];
}
