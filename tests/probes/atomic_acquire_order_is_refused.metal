// EXPECT: error other than memory_order_relaxed
// A non-relaxed order is refused rather than treated as relaxed. OpAtomicLoad
// is rejected by Vulkan under any non-relaxed order
// (VUID-StandaloneSpirv-MemorySemantics-10866, -10868 and -10870), and no
// atomic instruction gives the acquire and release guarantees the other orders
// ask for. Downgrading it to Relaxed would compile and run with weaker
// guarantees than the source requested, which is the failure this project has
// already hit with a float3 laid out as 12 bytes instead of 16.
#include <metal_stdlib>
using namespace metal;
kernel void atomic_acquire_order_is_refused(device atomic_uint *counter [[buffer(0)]]) {
  atomic_load_explicit(counter, memory_order_acquire);
}