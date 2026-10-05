// EXPECT: valid
// DISASM-MATCH: OpDecorate %_runtimearr__arr_float_uint_3 ArrayStride 12
// DISASM-MATCH: OpDecorate %_arr_float_uint_3 ArrayStride 4
// DISASM-NO-MATCH: ArrayStride 16
//
// A buffer of packed_float3 steps 12 bytes, where a buffer of float3 steps 16.
kernel void packed_buffer_stride(device packed_float3 *out [[buffer(0)]],
                                 device const packed_float3 *in [[buffer(1)]],
                                 uint index [[thread_position_in_grid]])
{
    out[index] = in[index];
}
