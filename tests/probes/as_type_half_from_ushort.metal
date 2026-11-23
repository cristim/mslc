// EXPECT: valid
// DISASM: OpBitcast %half
//
// as_type<half>(ushort) is one bit reinterpretation, as Apple lowers it.
kernel void as_type_half_from_ushort(device half* out [[buffer(0)]], device ushort* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = as_type<half>(in[i]);
}
