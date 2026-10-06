// EXPECT: error cannot store through "z"
//
// Apple: cannot assign to variable 'z' with const-qualified type 'const S'.
struct S {
    float x;
    float4 v;
    int n;
};

kernel void store_member_of_const_local_struct_rejected(device float *out [[buffer(0)]],
                                                   const device S *p [[buffer(1)]])
{
    const S z = p[0];
    z.n = 2;
    out[0] = z.x;
}
