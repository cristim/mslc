// EXPECT: valid
// DISASM-MATCH: %_struct_[0-9]+ = OpTypeStruct %_ptr_PhysicalStorageBuffer__struct_[0-9]+ %float
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 8
// A pointer member is a buffer's 8-byte GPU address, so q after it starts at 8.
struct S { const device float *p; float q; };
kernel void struct_pointer_member_const_device_offset(device float *o [[buffer(0)]],
    constant S &s [[buffer(1)]], uint i [[thread_position_in_grid]])
{ o[i] = s.q; }
