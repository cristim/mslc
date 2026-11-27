// EXPECT: valid
// A scalar is the diagonal, a matrix constant of the same shape converts element by element
// (float to half rounds), and a constructor takes columns * rows scalars in column order.
constant float2x3 diagonal = float2x3(7.0);
constant float3x3 wide = float3x3(0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9);
constant half3x3 narrow = half3x3(wide);
constant float2x3 zero = float2x3();

kernel void matrix_constant_diagonal_and_conversion(device float *out [[buffer(0)]])
{
    out[0] = diagonal[1][1];
    out[1] = float(narrow[0][0]);
    out[2] = zero[0][0];
}
