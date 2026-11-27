// EXPECT: valid
// A constant pointer and a device pointer are the same thing at the ABI level:
// both are members of the binding-0 block, which is what indium fills with
// addresses. There is no descriptor of its own to decorate, so there is no
// NonWritable to emit, and the shader's promise not to write through the
// constant one is a property of the source rather than of the module.
//
// The member offsets are the ABI: member k is the k-th buffer parameter in
// declaration order at byte offset 8 * k, not the Metal index, which here are 0
// for scale and 1 for out.
// DISASM-MATCH: OpTypeStruct %_ptr_PhysicalStorageBuffer.
// DISASM-MATCH: OpDecorate %[_0-9a-zA-Z]+ Block
// DISASM-MATCH: OpMemberDecorate %[_0-9a-zA-Z]+ 0 Offset 0
// DISASM-MATCH: OpMemberDecorate %[_0-9a-zA-Z]+ 1 Offset 8
// DISASM-NO-MATCH: NonWritable
kernel void constant_pointer(constant float* scale [[buffer(0)]],
                             device float* out [[buffer(1)]],
                             uint i [[thread_position_in_grid]])
{ out[i] = out[i] * scale[i]; }
