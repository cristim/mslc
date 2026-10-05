// EXPECT: error a vector is not implicitly converted to a vector of another component type
//
// Apple rejects storing an int2 into a float2 without a cast.
kernel void store_vector_component_mismatch_to_buffer_rejected(device float2 *out [[buffer(0)]], constant int2 *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = v[i];
}
