// EXPECT: valid
// DISASM-MATCH: OpDecorate %_runtimearr__arr_uchar_uint_2 ArrayStride 2
// DISASM-MATCH: OpINotEqual %v2bool
//
// Apple's bool2 is 2 bytes with alignment 2, a bool3 and a bool4 are 4 with
// alignment 4. The buffer holds the lanes as an array of bytes.
kernel void bool2_buffer_load(device uint *out [[buffer(0)]], device const bool2 *flags [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    if (flags[i].x) {
        out[i] = 1u;
    }
}
