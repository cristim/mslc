// EXPECT: error cannot store through "c"
//
// Apple: cannot assign to variable 'c' with const-qualified type 'const constant S &'.
struct S {
    float x;
    float4 v;
    int n;
};

kernel void store_member_through_constant_reference_rejected(device float *out [[buffer(0)]],
                                                   constant S &c [[buffer(1)]])
{
    c.n = 2;
    out[0] = c.x;
}
