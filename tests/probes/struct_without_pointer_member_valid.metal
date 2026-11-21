// EXPECT: valid
// DISASM: OpMemberDecorate %_struct_12 0 Offset 0
// DISASM: Offset 4
// A struct without a pointer member, and a pointer kernel argument, still compile (#346).
struct S { float p; float q; };
kernel void struct_without_pointer_member_valid(device float *o [[buffer(0)]], constant S &s [[buffer(1)]],
    uint i [[thread_position_in_grid]])
{ o[i] = s.q + s.p; }
