// EXPECT: valid
// DISASM: "metal_sub_header_has_include"
// Apple: __has_include answers true for each metal_* sub-header. mslc answers true for
// the eight it reads as <metal_stdlib>.
#if !__has_include(<metal_texture>) || !__has_include(<metal_math>) || !__has_include(<metal_common>) || !__has_include(<metal_geometric>) || !__has_include(<metal_integer>) || !__has_include(<metal_relational>) || !__has_include(<metal_graphics>) || !__has_include(<metal_types>)
#error a metal sub-header was not found
#endif
kernel void metal_sub_header_has_include(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
