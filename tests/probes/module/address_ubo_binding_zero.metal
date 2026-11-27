// EXPECT: valid
// The binding-0 ABI, which is what indium fills. Every buffer parameter is one
// member of a single Block-decorated Uniform block at set 0 binding 0, holding
// 8-byte PhysicalStorageBuffer pointers in declaration order at offset 8 * k.
// The addressing model is PhysicalStorageBuffer64 because a buffer is reached
// by its address rather than by a descriptor, and every access through such a
// pointer carries the Aligned memory operand, which spirv-val requires.
//
// The three Metal indices here are 0, 3 and 1, and the members are still 0, 1
// and 2: the member order is declaration order, not the Metal index.
// DISASM: OpCapability PhysicalStorageBufferAddresses
// DISASM: OpMemoryModel PhysicalStorageBuffer64 GLSL450
// DISASM-MATCH: OpDecorate %[_0-9a-zA-Z]+ DescriptorSet 0
// DISASM-MATCH: OpDecorate %[_0-9a-zA-Z]+ Binding 0
// DISASM-MATCH: OpMemberDecorate %[_0-9a-zA-Z]+ 0 Offset 0
// DISASM-MATCH: OpMemberDecorate %[_0-9a-zA-Z]+ 1 Offset 8
// DISASM-MATCH: OpMemberDecorate %[_0-9a-zA-Z]+ 2 Offset 16
// DISASM-MATCH: OpLoad %_ptr_PhysicalStorageBuffer
// DISASM-MATCH: OpLoad %float %[_0-9a-zA-Z]+ Aligned 4
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ Aligned 4
// DISASM-NO-MATCH: OpDecorate %[_0-9a-zA-Z]+ Binding 1
// DISASM-NO-MATCH: OpTypePointer StorageBuffer
kernel void address_ubo_binding_zero(device const float* a [[buffer(0)]],
                                     device float* out [[buffer(1)]],
                                     device const float* b [[buffer(3)]],
                                     uint i [[thread_position_in_grid]])
{
    out[i] = a[i] + b[i] * 2.0;
}
