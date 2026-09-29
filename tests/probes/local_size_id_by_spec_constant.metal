// EXPECT: valid
// The workgroup size is a spec constant rather than a literal, so indium can
// supply Metal's threadsPerThreadgroup through VkSpecializationInfo when it
// creates the pipeline. A literal would be baked in and the value the app asked
// for ignored.
//
// The mode is LocalSizeId, whose extra operands are ids, so it needs
// OpExecutionModeId: OpExecutionMode rejects id operands outright. The spec
// constant ids are 0, 1 and 2, which is the range indium reserves.
// DISASM: OpExecutionModeId
// DISASM: LocalSizeId
// DISASM-MATCH: OpDecorate %[0-9]+ SpecId 0
// DISASM-MATCH: OpDecorate %[0-9]+ SpecId 1
// DISASM-MATCH: OpDecorate %[0-9]+ SpecId 2
// DISASM-NO-MATCH: LocalSize 1 1 1
kernel void local_size_id_by_spec_constant(device const uint* in [[buffer(0)]],
                                            device uint* out [[buffer(1)]],
                                            uint i [[thread_position_in_grid]])
{ out[i] = in[i]; }
