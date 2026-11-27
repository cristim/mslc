// EXPECT: error "packed_double3" is not supported
//
// Apple's compiler has no packed double vectors either, and says
// unknown type name; mslc names the type it is refusing.
kernel void packed_double_rejected(device packed_double3 *out [[buffer(0)]],
                                   uint index [[thread_position_in_grid]])
{
}
