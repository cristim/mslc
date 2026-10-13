// EXPECT: error a matrix constant is multiplied by another matrix constant here
// Apple folds a matrix times a scalar too; mslc folds a product of matrices only and says so.
constant float2x2 base = float2x2(1.0);
constant float2x2 scaled = base * 2.0;

kernel void matrix_constant_scaled_not_folded(device float *out [[buffer(0)]])
{
    out[0] = scaled[0][0];
}
