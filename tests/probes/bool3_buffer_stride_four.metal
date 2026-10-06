// EXPECT: valid
// DISASM-MATCH: OpDecorate %_runtimearr__arr_uchar_uint_3 ArrayStride 4
// DISASM-MATCH: Aligned 4
// DISASM-MATCH: OpCompositeExtract %uchar %[0-9]+ 2
//
// A bool3 steps 4 bytes although it holds 3, as a float3 steps 16.
kernel void bool3_buffer_stride_four(device uint *out [[buffer(0)]], device const bool3 *flags [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool3 f = flags[i];
    out[i] = uint(f.z);
}
