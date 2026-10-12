// EXPECT: valid
kernel void matrix_from_scalar_spellings(device float *o [[buffer(0)]]) {
  float2x2 a = static_cast<float2x2>(2.0f);
  float2x2 b = (float2x2)3.0f;
  float4x4 c = float4x4(1);
  float2x2 d = float2x2(true);
  half2x2 e = half2x2(1.0);
  o[0] = a[0][0] + b[1][1] + c[3][3] + d[0][0] + float(e[0][0]);
}
