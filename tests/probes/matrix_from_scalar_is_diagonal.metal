// EXPECT: valid
// A single scalar is the diagonal, also for a matrix that is not square (Apple, measured on a GPU:
// float2x2(3) is 3 0 0 3, float3x3(2) has 2 on the diagonal, float2x3(5) is 5 0 0 0 5 0).
kernel void matrix_from_scalar_is_diagonal(device float *o [[buffer(0)]]) {
  float2x2 a = float2x2(3.0f);
  float2x3 w = float2x3(5.0f);
  o[0] = a[0][0] + a[1][1];
  o[1] = w[1][1];
}
