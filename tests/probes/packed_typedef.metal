// EXPECT: valid
// DISASM-MATCH: OpDecorate %_runtimearr__arr_float_uint_3 ArrayStride 12
typedef packed_float3 Position;

kernel void packed_typedef(device Position *out [[buffer(0)]],
                           device const Position *in [[buffer(1)]],
                           uint index [[thread_position_in_grid]])
{
    out[index] = in[index];
}
