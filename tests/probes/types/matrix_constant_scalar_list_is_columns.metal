// EXPECT: valid
// A braced list of scalars initialises one column per element, a scalar filling the whole
// column, and elements past the last column are dropped (Apple, measured on a GPU:
// float2x3 m = {1,2,3,4,5,6} is 1 1 1 2 2 2, and {1} is 1 1 1 0 0 0). The constructor, by
// contrast, takes the scalars in column order.
constant float2x3 broadcast = { 1, 2, 3, 4, 5, 6 };
constant float2x3 first = { 1 };
constant float2x3 ordered = float2x3(1, 2, 3, 4, 5, 6);

kernel void matrix_constant_scalar_list_is_columns(device float *out [[buffer(0)]])
{
    out[0] = broadcast[1][2];
    out[1] = first[1][0];
    out[2] = ordered[1][0];
}
