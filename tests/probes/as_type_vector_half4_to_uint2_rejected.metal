// EXPECT: error needs an operand with the same number of components
//
// as_type<uint2>(half4) is 8 bytes on both sides but changes the component count, which mslc does not lower yet.
kernel void as_type_vector_half4_to_uint2_rejected(device uint2* out [[buffer(0)]], device half4* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = as_type<uint2>(in[i]);
}
