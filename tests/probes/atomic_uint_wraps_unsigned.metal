// EXPECT: valid
// The unsigned wrap that a signed OpAtomicIAdd would get wrong. The counter is
// pushed to its last representable value and the fetch-add must return that and
// leave the counter at 0, which is what an unsigned add does and what a signed
// one would not: it would report -1 and leave the counter at INT_MIN.
//
// spirv-val accepts both lowerings, so this is pinned by the unsigned operand
// type rather than by validity.
// DISASM: OpAtomicIAdd %uint
// DISASM-NOT: OpAtomicIAdd %int
#include <metal_stdlib>
using namespace metal;
kernel void atomic_uint_wraps_unsigned(device atomic_uint *counter [[buffer(0)]],
                                       device uint *out [[buffer(1)]],
                                       uint gid [[thread_position_in_grid]])
{
    // 0xFFFFFFFF is the last unsigned value; written as a literal so the
    // emitter folds it to a uint constant rather than converting an int.
    if (gid == 0)
    {
        atomic_store_explicit(counter, 4294967295u, memory_order_relaxed);
    }

    out[gid] = atomic_fetch_add_explicit(counter, 1, memory_order_relaxed);
    if (gid == 0)
    {
        // Must read back 0: the add wrapped through the unsigned range.
        out[1] = atomic_load_explicit(counter, memory_order_relaxed);
    }
}