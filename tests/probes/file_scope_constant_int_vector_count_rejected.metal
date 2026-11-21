// EXPECT: error needs 4
//
// Apple rejects an int4 built from two or three components.
constant int4 k = int4(1, 2, 3);
kernel void file_scope_constant_int_vector_count_rejected(device int *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = k.x;
}
