// EXPECT: valid
// DISASM-MATCH: OpDecorate %_runtimearr__arr_half_uint_3 ArrayStride 6
// DISASM-MATCH: OpDecorate %_arr_half_uint_3 ArrayStride 2
// DISASM-MATCH: OpLoad %v3half %[0-9]+ Aligned 2
kernel void packed_half3_layout(device packed_half3 *out [[buffer(0)]],
                                device const packed_half3 *in [[buffer(1)]],
                                uint index [[thread_position_in_grid]])
{
    out[index] = in[index];
}
