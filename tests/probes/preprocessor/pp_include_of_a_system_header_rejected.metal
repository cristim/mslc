// EXPECT: error cannot honour #include <simd/quaternion.h>
// Only the metal and simd headers listed in the diagnostic are provided.
#include <simd/quaternion.h>
kernel void pp_include_of_a_system_header_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
