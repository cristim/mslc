// EXPECT: valid
// DISASM: OpBitcast %v3uint
//
// as_type<uint3>(float3) is one bit reinterpretation of the same component count, as Apple lowers it.
kernel void as_type_uint3_from_float3(device uint3* out [[buffer(0)]], device float3* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = as_type<uint3>(in[i]);
}
