// EXPECT: error other than memory_order_relaxed
// Metal's atomic builtins take memory_order_relaxed and nothing else:
// xcrun -sdk macosx metal reports "use of undeclared identifier
// 'memory_order_acquire'" for every other spelling. mslc reports the same way
// rather than accepting an order Apple has no name for.
//
// OpAtomicLoad under a non-relaxed order is also refused by Vulkan
// (VUID-StandaloneSpirv-MemorySemantics-10868 and -10870), so accepting one
// would not lower to a legal module anyway.
#include <metal_stdlib>
using namespace metal;
kernel void atomic_memory_order_must_be_relaxed(device atomic_uint *counter [[buffer(0)]]) {
  atomic_load_explicit(counter, memory_order_acquire);
}
