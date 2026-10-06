// EXPECT: valid
struct S {
    float x;
    float4 v;
    int n;
};

kernel void store_threadgroup_struct_member_is_writable(device float *out [[buffer(0)]],
                                                   const device S *p [[buffer(1)]])
{
    threadgroup S t;
    t.x = p[0].x;
    t.v.y = 3.0;
    out[0] = t.x + t.v.y;
}
