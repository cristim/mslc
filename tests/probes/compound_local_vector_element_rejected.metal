// EXPECT: error a compound assignment to an element of the local vector "v" is not lowered yet
//
// Indexing a local vector is #53.
kernel void compound_local_vector_element_rejected(device float4 *out [[buffer(0)]],
                                                   uint i [[thread_position_in_grid]])
{
    float4 v = out[i];
    v[1] += 1.0f;
    out[i] = v;
}
