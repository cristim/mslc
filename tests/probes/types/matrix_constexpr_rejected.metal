// EXPECT: error constexpr variable "m" must be initialized by a constant expression
// Apple has no constexpr constructor for a matrix: this is an error for every initialiser.
constexpr constant float2x2 m = float2x2(1.0);

kernel void matrix_constexpr_rejected(device float *out [[buffer(0)]])
{
    out[0] = m[0][0];
}
