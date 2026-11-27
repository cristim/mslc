// EXPECT: valid
// float2x2(m), static_cast and the C-style cast copy a matrix of the same shape; half converts.
kernel void matrix_from_matrix_copies(device float *o [[buffer(0)]]) {
  float2x2 s = float2x2(float2(1, 2), float2(3, 4));
  float2x2 a = float2x2(s);
  float2x2 b = static_cast<float2x2>(s);
  float2x2 c = (float2x2)s;
  half2x2 h = half2x2(half2(1, 2), half2(3, 4));
  float2x2 d = float2x2(h);
  o[0] = a[0][0] + b[0][1] + c[1][0] + d[1][1];
}
