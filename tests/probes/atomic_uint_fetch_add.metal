// EXPECT: valid
// The three atomic operations #182 lowers, each on its own counter. One kernel
// per module, since indium specialises one set of workgroup constants per
// module.
//
// Apple accepts this with no diagnostics. The add is unsigned, which the
// disassembly below pins: an OpAtomicIAdd on a signed int would validate and
// wrap at 2^31 instead of 2^32.
// DISASM-NOT: OpBitcast
// DISASM: OpAtomicIAdd %uint
// DISASM: OpAtomicStore
// DISASM: OpAtomicLoad %uint
//
// atomic_uint and the memory_order_* names come from <metal_stdlib> alone;
// <metal_atomic> also declares them, but mslc does not provide that header and
// reports it rather than ignoring it. Both spellings compile with Apple's
// compiler and the measured callers use the stdlib one.
#include <metal_stdlib>
using namespace metal;
kernel void atomic_uint_fetch_add(device atomic_uint *counter [[buffer(0)]],
                                 device uint *out [[buffer(1)]],
                                 uint gid [[thread_position_in_grid]])
{
    // The returned value is the counter's value before this fetch-add.
    uint previous = atomic_fetch_add_explicit(counter, 1, memory_order_relaxed);
    out[gid] = previous;

    // The other two spellings, at the same relaxed order.
    atomic_store_explicit(counter, previous, memory_order_relaxed);
    out[gid] = atomic_load_explicit(counter, memory_order_relaxed);
}