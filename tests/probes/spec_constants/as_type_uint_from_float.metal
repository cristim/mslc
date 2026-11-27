// EXPECT: valid
// DISASM: OpBitcast %uint
//
// as_type<uint>(float) is one bit reinterpretation, as Apple lowers it.
kernel void as_type_uint_from_float(device uint* out [[buffer(0)]], device float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = as_type<uint>(in[i]);
}
