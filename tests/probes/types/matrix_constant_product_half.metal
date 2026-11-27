// EXPECT: valid
// REFLECT-NOT: error
// A product of two half matrix constants is folded with a rounding to half after each
// product and each sum, which is what Apple's compiler produced on a GPU.
constant half3x3 a = half3x3(half3(0.5689h, 0.0944h, 0.0496h), half3(0.3467h, 0.8484h, 0.1416h),
                             half3(0.0843h, 0.0572h, 0.8088h));
constant half3x3 b = half3x3(half3(0.6370h, 0.2627h, 0.0h), half3(0.1446h, 0.6780h, 0.0281h),
                             half3(0.1689h, 0.0593h, 1.0610h));
constant half3x3 both = a * b;

kernel void matrix_constant_product_half(device half3 *out [[buffer(0)]],
                                         uint i [[thread_position_in_grid]])
{
    out[i] = both * half3(1.0h);
}
