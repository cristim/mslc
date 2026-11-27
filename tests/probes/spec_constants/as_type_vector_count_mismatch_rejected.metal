// EXPECT: error of a three-component vector is lowered only to another three-component vector
//
// as_type<uint4>(float3) changes the component count from 3 to 4, which Apple rejects.
kernel void as_type_vector_count_mismatch_rejected(device uint4* out [[buffer(0)]], device float3* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = as_type<uint4>(in[i]);
}
