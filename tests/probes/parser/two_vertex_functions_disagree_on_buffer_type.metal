// EXPECT: valid
// DISASM-MATCH: DescriptorSet 0.*DescriptorSet 0
// Two entry points of one stage with the same number of buffers at different
// pointee types (float and int). Sharing one block would give an access chain
// whose result type is not the member's type; each entry point gets its own
// block at set 0, binding 0 instead. See two_vertex_functions_disagree_on_buffers.
vertex void vertex_float_buffer(device float *p [[buffer(0)]],
                                  uint index [[vertex_id]])
{
    p[index] = 1.0;
}

vertex void vertex_int_buffer(device int *p [[buffer(0)]],
                                uint index [[vertex_id]])
{
    p[index] = 1;
}
