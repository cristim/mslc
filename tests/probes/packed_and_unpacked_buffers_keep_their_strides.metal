// EXPECT: valid
// DISASM-MATCH: OpDecorate %_runtimearr_v3float ArrayStride 16
// DISASM-MATCH: OpDecorate %_runtimearr__arr_float_uint_3 ArrayStride 12
//
// One SPIR-V vector type, two layouts: the two runtime arrays below have to be
// different types with different strides.
kernel void packed_and_unpacked_buffers_keep_their_strides(
    device float3 *wide [[buffer(0)]],
    device packed_float3 *tight [[buffer(1)]],
    uint index [[thread_position_in_grid]])
{
    wide[index] = tight[index];
}
