// EXPECT: valid
// DISASM: OpStore
struct S {
    float x;
    float4 v;
    int n;
};

kernel void store_local_copy_of_constant_struct_is_writable(device float *out [[buffer(0)]],
                                                   constant S *p [[buffer(1)]])
{
    S z = p[0];
    z.x = 2.0;
    z.v.y = 3.0;
    z.v.zw = float2(4.0);
    out[0] = z.x + z.v.y + z.v.w;
}
