// EXPECT: error cannot store through "p"
//
// Apple: no viable overloaded '='.
struct S {
    float x;
    float4 v;
    int n;
};

kernel void store_whole_struct_to_constant_buffer_rejected(device float *out [[buffer(0)]],
                                                   constant S *p [[buffer(1)]],
                                                   uint i [[thread_position_in_grid]])
{
    p[i] = p[0];
    out[0] = p[0].x;
}
