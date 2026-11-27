// EXPECT: error no matching conversion from this value
// Apple: "no matching conversion for functional-style cast from float2 to float2x2".
kernel void matrix_from_vector_rejected(device float *o [[buffer(0)]]) {
  float2x2 m = float2x2(float2(1, 2));
  o[0] = m[0][0];
}
