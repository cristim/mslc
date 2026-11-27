// EXPECT: valid
// A kernel's workgroup size is three spec constants, and their default is the
// size the caller asked for rather than zero. indium overrides all three through
// VkSpecializationInfo, so the default only has to be legal, and 0, 0, 0 passes
// spirv-val and is then rejected when the pipeline is created. The reflection has
// to agree with the module, or it describes a size the module does not have.
// DISASM: LocalSizeId
// DISASM-MATCH: OpSpecConstant %uint 1
// DISASM-NO-MATCH: OpSpecConstant %uint 0
// REFLECT: "local_size": [1, 1, 1]
kernel void local_size_default_is_legal(device const uint* in [[buffer(0)]],
                                        device uint* out [[buffer(1)]],
                                        uint i [[thread_position_in_grid]])
{ out[i] = in[i]; }
