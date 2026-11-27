// EXPECT: valid
// DISASM-MATCH: DescriptorSet 0.*DescriptorSet 0
// Two entry points of one stage in one source, binding a different number of
// buffers. A module has one typed address block per set, so the two cannot
// share one: each gets its own variable at set 0, binding 0. indium creates a
// pipeline from one entry point and Vulkan only needs (set, binding) unique
// among the variables that entry point uses, so this is a valid module. It used
// to be refused with "two entry points sharing descriptor set 0 bind different
// buffers", which stopped iTermMargin and iTermMark. Both variables have to
// appear: a module with one would index one entry point's block with the
// other's member list.
vertex void vertex_two_buffers(device float *a [[buffer(0)]],
                                 device float *b [[buffer(1)]],
                                 uint index [[vertex_id]])
{
    a[index] = b[index] * 2.0;
}

vertex void vertex_three_buffers(device float *c [[buffer(0)]],
                                   device float *d [[buffer(1)]],
                                   device float *e [[buffer(2)]],
                                   uint index [[vertex_id]])
{
    c[index] = d[index] + e[index];
}
