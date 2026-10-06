// EXPECT: error the index of a vector has to be an integer or a bool
kernel void vector_element_float_index_rejected(device float *out [[buffer(0)]],
                                                device const float4 *in [[buffer(1)]])
{
    float4 v = in[0];
    out[0] = v[1.0f];
}
