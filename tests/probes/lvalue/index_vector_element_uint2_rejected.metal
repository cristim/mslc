// EXPECT: error the index of a vector has to be an integer or a bool
//
// A uint2 is not an index of a vector. (#54)
struct S { float a; };
kernel void index_vector_element_uint2_rejected(device float4 *out [[buffer(0)]],
               device const float4 *in [[buffer(1)]],
               device const float4x4 *mats [[buffer(2)]])
{
    float4x4 m = mats[0];
    float4 v = in[0];
    uint2 k = uint2(1, 0);
    out[0] = float4(v[k]);
}
