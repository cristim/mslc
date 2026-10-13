// EXPECT: valid
// A local matrix from columns * rows scalars in column order, and a local braced list in which
// a scalar fills a column and the columns not written are zero.
kernel void matrix_construct_scalars(device float *out [[buffer(0)]])
{
  float x = 9.0;
  float3x3 a = float3x3(1, 2, 3, 4, 5, 6, 7, 8, x);
  const float2x3 b = { 1 };
  out[0] = a[2][2];
  out[1] = b[0][2];
}
