// EXPECT: error "packed_float8" is not supported
//
// Apple spells packed vectors of 8 and 16 components; mslc has the 2 to 4 that
// every other vector type here is limited to.
kernel void packed_wide_vector_rejected(device packed_float8 *out [[buffer(0)]],
                                        uint index [[thread_position_in_grid]])
{
}
