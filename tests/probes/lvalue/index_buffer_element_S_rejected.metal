// EXPECT: error the index of a buffer element has to be an integer or a bool
//
// A S is not an index; it used to be relabelled as an unsigned integer. (#54)
struct S { float a; };
kernel void index_buffer_element_S_rejected(device float4 *out [[buffer(0)]],
               device const float4 *in [[buffer(1)]],
               device const float4x4 *mats [[buffer(2)]])
{
    float4x4 m = mats[0];
    float4 v = in[0];
    S k; k.a = 1.0f;
    out[0] = in[k];
}
