// EXPECT: valid
// DISASM-MATCH: OpLoad %v4float %[0-9]+ Aligned 4
// DISASM-NO-MATCH: Aligned 16
// DISASM-MATCH: OpDecorate %_runtimearr__arr_float_uint_4 ArrayStride 16
//
// A packed_float4 is the size of a float4 and not its alignment.
kernel void packed_float4_is_aligned_to_a_float(device packed_float4 *out [[buffer(0)]],
                                                device const packed_float4 *in [[buffer(1)]],
                                                uint index [[thread_position_in_grid]])
{
    out[index] = in[index];
}
