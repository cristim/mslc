// EXPECT: error has to be a device atomic_uint pointer
// The first argument has to be a device atomic_uint pointer, so a plain uint
// buffer pointer is refused rather than treated as an atomic counter: applying
// an atomic to non-atomic storage is undefined, and accepting it would compile
// and then race.
#include <metal_stdlib>
using namespace metal;
kernel void atomic_on_a_plain_pointer_is_refused(device uint *counter [[buffer(0)]]) {
  atomic_fetch_add_explicit(counter, 1, memory_order_relaxed);
}