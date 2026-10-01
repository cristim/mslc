// EXPECT: error two entry points sharing descriptor set 0 bind different buffers
// The same count, different types. This is the case that comparing only the
// number of buffers would miss: the block has as many members as the first
// function builds it, so the second finds member 0 present and indexes it, and
// what it reads is a pointer to an int where the source says a pointer to a
// float.
//
// spirv-val catches the mismatch in the access chain's result type, so this
// would be a rejected module rather than a wrong one. That is the better of the
// two outcomes, but it is a rejection, and a rejection of a source xcrun metal
// accepts is mslc being stricter than the reference for a shape it can express.
// Comparing the pointee types turns the rejection into a diagnostic that names
// the reason.
//
// Vertex functions for the reason in two_vertex_functions_disagree_on_buffers:
// two kernels in one source is refused earlier, for the workgroup-size spec
// constants.
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
