// EXPECT: valid
// A vertex shader binds at set 0, the other half of the stage-dependent set, so
// the choice is pinned on both sides rather than only on the fragment one.
// This one passes before and after: it is a guard that the rule did not become a
// blanket set 1, which the fragment probe alone would not catch.
// DISASM: OpEntryPoint Vertex
// DISASM-MATCH: OpDecorate %[_0-9a-zA-Z]+ DescriptorSet 0
// DISASM: Binding 0
// DISASM-NO-MATCH: DescriptorSet 1
vertex void vertex_uses_set_zero(device float* out [[buffer(0)]],
                                uint vid [[vertex_id]])
{ out[vid] = 1.0; }
