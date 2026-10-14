// EXPECT: error which is in constant memory
// A counter in constant memory cannot be incremented atomically. mslc refuses
// it rather than emitting an atomic against read-only storage, which spirv-val
// accepts and a driver rejects or misbehaves on.
#include <metal_stdlib>
using namespace metal;
kernel void atomic_on_a_constant_buffer_is_refused(constant atomic_uint *counter [[buffer(0)]]) {
  atomic_fetch_add_explicit(counter, 1, memory_order_relaxed);
}