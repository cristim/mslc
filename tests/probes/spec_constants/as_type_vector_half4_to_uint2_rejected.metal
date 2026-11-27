// EXPECT: error as_type<uint2> needs an operand of the same size: it is 8 bytes, and the operand is 4
//
// as_type<uint2>(half2) is 8 bytes vs 4 bytes and is rejected.
kernel void as_type_vector_half4_to_uint2_rejected(device uint2* out [[buffer(0)]], device half2* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = as_type<uint2>(in[i]);
}
