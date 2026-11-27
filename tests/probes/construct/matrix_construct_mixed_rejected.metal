// EXPECT: error no matching constructor for float2x2 built from 3 values
// Apple: scalars alone or one column vector per column, not a mixture.
kernel void matrix_construct_mixed_rejected(device float *out [[buffer(0)]])
{
  float2x2 a = float2x2(float2(1, 2), 3, 4);
  out[0] = a[0][0];
}
