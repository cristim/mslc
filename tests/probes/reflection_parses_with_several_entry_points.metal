// EXPECT: valid
// REFLECT: "reflection_version": 2
// REFLECT: "entry_points"
// REFLECT: "name": "vertex_with_a_local_and_a_loop"
// REFLECT: "stage": "vertex"
// REFLECT: "member": 0
// REFLECT: "member": 1
// REFLECT: "metal_index": 1
//
// The reflection is a document a reader parses, and a document that does not
// parse is not a reflection. Every binding was emitted with a trailing comma, so
// the array ended "... , ]" and Python's json.load refused it:
//
//   json.decoder.JSONDecodeError: Illegal trailing comma before end of array
//
// So the separator is now written before each entry rather than after it, which
// is why the first binding carries no comma. Nothing else in the shape changes.
//
// The stage is spelled "vertex" and the workgroup size is reported for every
// entry point, including the two graphics ones that have no workgroup. That was
// part of the same reshape, neither was mentioned in the branch description, and
// both are pinned here so a later change to the shape is a visible one. Reporting
// local_size for a vertex entry point is misleading rather than wrong, and #40
// owns the reflection's contract, so it is left as is and named here.
//
// Two vertex functions, so there are two entries in entry_points and each has its
// own bindings with the members numbered from 0. The buffer indices are 0 and 1
// in both, and the members are 0 and 1, which is what says the block is shared
// per set and indexed in declaration order.
vertex void vertex_with_a_local_and_a_loop(device uint *in [[buffer(0)]],
                                          device uint *out [[buffer(1)]],
                                          uint index [[vertex_id]])
{
    uint total = 0u;
    for (uint step = 0u; step < 4u; step = step + 1u) {
        total = total + in[step];
    }
    out[index] = total;
}

vertex void vertex_without_locals(device uint *in [[buffer(0)]],
                                  device uint *out [[buffer(1)]],
                                  uint index [[vertex_id]])
{
    out[index] = in[index];
}
