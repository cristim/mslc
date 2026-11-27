// EXPECT: valid
// DISASM: OpBitcast %ushort
//
// as_type<ushort>(half) is one bit reinterpretation, as Apple lowers it.
kernel void as_type_ushort_from_half(device ushort* out [[buffer(0)]], device half* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = as_type<ushort>(in[i]);
}
