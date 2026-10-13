// EXPECT: error a column of float2x3 takes 3 values in braces, found 2
// Apple: "chosen constructor is explicit in copy-initialization". A braced column gives all of
// its components.
constant float2x3 m = { { 1, 2, 3 }, { 4, 5 } };

kernel void matrix_constant_short_column_rejected(device float *out [[buffer(0)]])
{
    out[0] = m[0][0];
}
