// EXPECT: valid
// DISASM: = OpConvertSToF %v3float
// DISASM-MATCH: OpCompositeExtract %float %[0-9]+ 0
kernel void static_cast_member_access(device float* out [[buffer(0)]], constant int3* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = static_cast<float3>(in[i]).x;
}
