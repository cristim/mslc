// EXPECT: error a vector piece of float4 has to have its component type
//
// Apple reports "no matching constructor" for an int3 piece of a float4; only
// a scalar piece is converted.
kernel void construct_vector_piece_of_another_type_rejected(device float4 *out [[buffer(0)]], constant int3 *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = float4(v[i], 1.0);
}
