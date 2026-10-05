// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 4
struct Packed
{
    float lead;
    packed_float3 value;
};

kernel void packed_constant_buffer(device float3 *out [[buffer(0)]],
                                   constant Packed &packed [[buffer(1)]],
                                   uint index [[thread_position_in_grid]])
{
    out[index] = packed.value;
}
