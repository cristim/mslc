// EXPECT: error indexing an expression is not supported yet
//
// Apple accepts "(*p)[1]". mslc indexes a parameter directly and nothing else.
kernel void deref_then_index_not_lowered(device float* out [[buffer(0)]],
                                         device const float4* in [[buffer(1)]])
{
    out[0] = (*in)[1];
}
