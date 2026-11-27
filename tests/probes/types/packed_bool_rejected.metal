// EXPECT: error "packed_bool3" is not supported
//
// Apple's compiler has packed_bool vectors. A bool has no storage layout in
// mslc, and an unpacked bool3 in a buffer is not given one either.
struct Flags
{
    packed_bool3 enabled;
};

kernel void packed_bool_rejected(device Flags *flags [[buffer(0)]],
                                 uint index [[thread_position_in_grid]])
{
}
