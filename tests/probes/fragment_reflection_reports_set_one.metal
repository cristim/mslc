// EXPECT: valid
// The reflection is the contract indium consumes, and nothing else in the suite
// looked at it, so a stage-dependent descriptor set reached the module while the
// reflection still said set 0: a fragment shader's buffers were all reported in
// a set indium never binds. The set here has to be the one the module declares.
//
// The Metal indices are 0, 3 and 1 and the members are 0, 1 and 2, since a member
// is the parameter's position in declaration order and the Metal index is the slot
// the app bound at.
// DISASM: OpEntryPoint Fragment
// DISASM-MATCH: OpDecorate %[_0-9a-zA-Z]+ DescriptorSet 1
// DISASM-NO-MATCH: DescriptorSet 0
// REFLECT: "set": 1
// REFLECT: "member": 0
// REFLECT: "member": 1
// REFLECT: "member": 2
// REFLECT: "metal_index": 3
// REFLECT-NOT: "set": 0
fragment void fragment_reflection_reports_set_one(device const float* a [[buffer(0)]],
                                                 device float* out [[buffer(1)]],
                                                 device const float* b [[buffer(3)]])
{ out[0] = a[0] + b[0]; }
