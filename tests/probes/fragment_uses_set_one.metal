// EXPECT: valid
// indium splits descriptor sets by stage: set 0 from the vertex function, set 1
// from the fragment function. A fragment shader's address block therefore has to
// be in set 1 or indium never binds it, which is the opposite of the kernel
// case. A fragment entry point also needs OriginUpperLeft, since Vulkan requires
// one of the two origin modes and Metal's framebuffer origin is upper left.
//
// This is a deliberately bare fragment shader. It has no colour output, because
// [[color(n)]] and a fragment return value are M2 work; all this needs is the
// execution model, which is what chooses the set.
// DISASM: OpEntryPoint Fragment
// DISASM-MATCH: OpExecutionMode %[0-9]+ OriginUpperLeft
// DISASM-MATCH: OpDecorate %[_0-9a-zA-Z]+ DescriptorSet 1
// DISASM: Binding 0
// DISASM-NO-MATCH: DescriptorSet 0
fragment void fragment_uses_set_one(device float* out [[buffer(0)]])
{ out[0] = 1.0; }
