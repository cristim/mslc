// EXPECT: valid
//
// __has_include answers for <simd/simd.h>.
#if !__has_include(<simd/simd.h>)
#error the header is built in
#endif
kernel void simd_include_has_include(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
