// EXPECT: valid
// DISASM: = OpConvertSToF %v3float
// DISASM: = OpVectorShuffle %v2float
// DISASM: = OpVectorExtractDynamic %float
// DISASM: = OpConvertFToS %int
kernel void static_cast_nested_postfix_chain(device int* out [[buffer(0)]], constant int3* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = static_cast<int>(static_cast<float3>(in[i]).yz[i % 2]);
}
