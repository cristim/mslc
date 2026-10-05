// EXPECT: error cannot honour #include <simd/packed.h>
//
// Only <simd/simd.h> is built in. <simd/packed.h> is a real Metal header that Apple accepts, and honouring it unread would drop what it declares.
#include <simd/packed.h>
kernel void simd_packed_header_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
