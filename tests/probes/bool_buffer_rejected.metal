// EXPECT: error parameter "flags" is a buffer of bool*, which mslc does not lay out yet
//
// Metal gives a bool one byte in a buffer. A bool in a SPIR-V storage buffer has
// no byte layout, and lavapipe reads it as a word, so a flag array that holds
// 1 0 1 1 comes back all true. Rejecting it is the alternative to that.
kernel void bool_buffer_rejected(device uint *out [[buffer(0)]], device bool *flags [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    if (flags[i]) {
        out[i] = 1u;
    }
}
