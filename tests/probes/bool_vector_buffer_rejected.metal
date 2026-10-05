// EXPECT: error parameter "flags" is a buffer of bool2*, which mslc does not lay out yet
//
// Apple's bool2 is 2 bytes with alignment 2, bool3 and bool4 are 4 with alignment 4.
kernel void bool_vector_buffer_rejected(device uint *out [[buffer(0)]], device bool2 *flags [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    if (flags[i].x) {
        out[i] = 1u;
    }
}
