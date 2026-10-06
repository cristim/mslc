// EXPECT: valid
// DISASM-MATCH: = OpCompositeExtract %float %[0-9]+ 1
//
// "(*in)[1]" is a vector element; Apple accepts it and so does mslc (#53).
kernel void deref_then_index_vector_element(device float* out [[buffer(0)]],
                                            device const float4* in [[buffer(1)]])
{
    out[0] = (*in)[1];
}
