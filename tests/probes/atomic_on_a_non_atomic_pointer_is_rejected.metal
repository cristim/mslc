// EXPECT: error is not one
// An atomic function only takes an atomic. A plain device uint* is not one,
// and Apple refuses the call outright ("no matching function for call to
// 'atomic_fetch_add_explicit'") rather than applying an atomic to ordinary
// storage, which is undefined behaviour rather than merely unusual.
//
// This is the pointer half of the guard. The probe beside it covers the
// ordinary-access half, and neither was covered by a needle before: the guard
// in atomicAddress() rejected this case, but no probe failed if it stopped
// doing so.
#include <metal_stdlib>
using namespace metal;

kernel void atomic_on_a_non_atomic_pointer_is_rejected(device uint *counter [[buffer(0)]],
                                                      device uint *out [[buffer(1)]])
{
  out[0] = atomic_fetch_add_explicit(counter, 1, memory_order_relaxed);
}