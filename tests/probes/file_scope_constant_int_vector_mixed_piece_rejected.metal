// EXPECT: error unless it is the only argument
//
// Apple rejects a vector piece of another scalar type beside other arguments
// ("int4(uint2(1, 2), 3, 4)"); it converts one only as the whole argument list.
constant int4 k = int4(uint2(1, 2), 3, 4);
kernel void file_scope_constant_int_vector_mixed_piece_rejected(device int *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = k.x;
}
