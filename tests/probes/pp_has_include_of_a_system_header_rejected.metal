// EXPECT: error __has_include(<simd/packed.h>) cannot be answered
// mslc has no system header search.
#if __has_include(<simd/packed.h>)
#endif
kernel void pp_has_include_of_a_system_header_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
