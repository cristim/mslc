// EXPECT: error takes memory_order_relaxed
// Metal's atomic builtins take memory_order_relaxed and nothing else:
// xcrun -sdk macosx metal reports "use of undeclared identifier
// 'memory_order_acquire'" for every other spelling. mslc reports the same way
// rather than accepting an order Apple has no name for.
//
// It is also the only order mslc lowers, because a load carrying acquire
// semantics has no single OpAtomicLoad: Vulkan accepts OpAtomicLoad at Acquire
// with a storage-class semantics bit set
// (VUID-StandaloneSpirv-MemorySemantics-10870), but refuses Release and
// AcquireRelease outright (-10868), so mapping the rest is not a single
// instruction rewrite.
#include <metal_stdlib>
using namespace metal;
kernel void atomic_memory_order_must_be_relaxed(device atomic_uint *counter [[buffer(0)]]) {
  atomic_load_explicit(counter, memory_order_acquire);
}
