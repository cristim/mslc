// EXPECT: error two entry points in the same stage bind different buffers
// Two kernels in one source binding different buffers. indium packs an entry
// point's buffer addresses into one address block at binding 0, and a module
// has one such variable per set, so two kernels in the same stage share it. The
// members are typed and their count is fixed when the first kernel builds it,
// and a second kernel indexing it differently produces an access chain whose
// result type is not the type the member holds.
//
// Two ways that happens, and the first is the one worth reading twice. Same
// count, different pointee type:
//
//   error: OpAccessChain result type does not match the type that results
//          from indexing into the base. (The types must be the exact same Id)
//
// Same stage, different count, which reads past the member list instead:
//
//   error: Index 2 is out of bounds: this structure has 2 members.
//
// Both are hard validation failures, so a rejected module is the outcome either
// way. What is not acceptable is a module that passes validation and reads the
// wrong address, and comparing only the count would have left the first case
// emitting one.
kernel void k_two_buffers(device float *a [[buffer(0)]],
                          device float *b [[buffer(1)]],
                          uint i [[thread_position_in_grid]])
{
    a[i] = b[i] * 2.0;
}

kernel void k_three_buffers(device float *c [[buffer(0)]],
                            device float *d [[buffer(1)]],
                            device float *e [[buffer(2)]],
                            uint i [[thread_position_in_grid]])
{
    c[i] = d[i] + e[i];
}
