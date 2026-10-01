// EXPECT: error two entry points sharing descriptor set 0 bind different buffers
// Two entry points in one source binding different buffers. indium packs an
// entry point's buffer addresses into one address block at binding 0, and a
// module has one such variable per set, so two entry points in the same stage
// share it. The members are typed and their count is fixed when the first
// function builds it, and a second one indexing it differently produces an
// access chain whose result type is not the type the member holds.
//
// Two ways that happens, and the first is the one worth reading twice. Same
// count, different pointee type:
//
//   error: OpAccessChain result type does not match the type that results
//          from indexing into the base. (The types must be the exact same Id)
//
// Same set, different count, which reads past the member list instead:
//
//   error: Index 2 is out of bounds: this structure has 2 members.
//
// Both are hard validation failures, so a rejected module is the outcome either
// way. What is not acceptable is a module that passes validation and reads the
// wrong address, and comparing only the count would have left the first case
// emitting one.
//
// These are vertex functions because two kernels in one source is a different
// refusal: a module has one set of three workgroup-size spec constants at
// SpecId 0, 1, 2 and indium specialises them once per pipeline, so mslc refuses
// a second kernel outright. Two vertex functions share a set the same way and
// reach this check, which is what makes the disagreement reachable at all.
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
