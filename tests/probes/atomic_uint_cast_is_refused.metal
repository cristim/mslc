// EXPECT: error a cast to atomic_uint is not supported
// atomic_uint names a type for a buffer parameter; a value cannot be converted
// to one. Apple reports "no matching conversion for C-style cast from 'int' to
// 'metal::atomic_uint'" for the C-style spelling and refuses the named one for
// the same reason.
#include <metal_stdlib>
using namespace metal;
kernel void atomic_uint_cast_is_refused(device uint *out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  out[gid] = (uint)static_cast<atomic_uint>(3);
}