// EXPECT: valid
// DISASM-MATCH: OpSelect %uchar %[0-9]+ %uchar_1 %uchar_0
// DISASM-MATCH: OpStore %[0-9]+ %[0-9]+ Aligned 1
//
// A stored bool is the byte 1 or 0, never the bool's own bits.
kernel void bool_buffer_store(device bool *flags [[buffer(0)]], device const uint *in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    flags[i] = in[i] > 3u;
}
