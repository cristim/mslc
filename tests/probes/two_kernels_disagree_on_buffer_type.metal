// EXPECT: error two entry points in the same stage bind different buffers
// The same count, different types. This is the case that comparing only the
// number of buffers would miss: the block has as many members as the first
// entry point declared, the second entry point indexes them all, and nothing
// looks wrong until the access chain's result type is compared against the type
// the member actually holds.
//
// Reproduced before the check compared types, not just the count:
//
//   OpAccessChain %_ptr_PhysicalStorageBuffer__struct_64 %25 %uint_0_2
//   error: OpAccessChain result type does not match the type that results from
//          indexing into the base. (The types must be the exact same Id)
//
// A hard validation failure, so a rejected module rather than a wrong answer. The
// point of catching it here is that a rejected module is a diagnostic the author
// can read, and this one names the constraint.
kernel void k_float_buffers(device float *a [[buffer(0)]],
                            device float *b [[buffer(1)]],
                            uint i [[thread_position_in_grid]])
{
    a[i] = b[i] * 2.0;
}

kernel void k_vector_buffers(device int4 *c [[buffer(0)]],
                             device int4 *d [[buffer(1)]],
                             uint i [[thread_position_in_grid]])
{
    c[i] = d[i];
}
