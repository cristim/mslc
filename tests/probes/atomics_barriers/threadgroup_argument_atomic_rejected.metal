// EXPECT: error points at an atomic
//
// Apple accepts it. mslc does not lower a threadgroup atomic array either (known-issues.md).
kernel void threadgroup_argument_atomic_rejected(threadgroup atomic_uint* a [[threadgroup(0)]]) { atomic_store_explicit(a, 1u, memory_order_relaxed); }
