// EXPECT: error non-static data member
//
// A member declared above hides the constant, and is not one (Apple: invalid use of non-static
// data member 'N').
constant int N = 3;
struct S { int N; float a[N]; };
kernel void array_length_constant_struct_member_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = 1.0;
}
