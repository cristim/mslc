// EXPECT: error declares before this one
//
// A constant declared after the declaration that names it is not in scope there (Apple:
// "use of undeclared identifier").
struct Late { float a[N]; };
constant int N = 3;
kernel void array_length_named_constant_undeclared_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = 1.0;
}
