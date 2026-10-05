// EXPECT: valid
// DISASM-MATCH: OpDecorate %_runtimearr__arr_uchar_uint_3 ArrayStride 3
// DISASM-MATCH: OpDecorate %_arr_uchar_uint_3 ArrayStride 1
// DISASM-MATCH: OpLoad %v3uchar %[0-9]+ Aligned 1
kernel void packed_uchar3_layout(device packed_uchar3 *out [[buffer(0)]],
                                 device const packed_uchar3 *in [[buffer(1)]],
                                 uint index [[thread_position_in_grid]])
{
    out[index] = in[index];
}
