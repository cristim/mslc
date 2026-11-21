// EXPECT: valid
// DISASM-MATCH: OpDecorate %[0-9]+ Location 0[^0-9]
// DISASM-MATCH: OpDecorate %[0-9]+ Location 2[^0-9]
// DISASM-NO-MATCH: Location 1[^0-9]
//
// An attribute index may be an element of a constexpr integer array, as the
// MSL that Unity's shader translator writes does it: the array of remapped
// colour indices has a file-scope brace initialiser and the field says
// [[color(table[n])]]. The elements are 0 and 2 here, so the two fields are
// Locations 0 and 2 and nothing is Location 1.
#include <metal_stdlib>
using namespace metal;
#define REMAP {0, 2, 1}
constexpr constant uint remap[] = REMAP;
struct Out { float4 a [[color(remap[0])]]; float4 b [[color(remap[1])]]; };
fragment Out fragment_color_index_constant_array_element()
{ Out o; o.a = float4(1.0); o.b = float4(2.0); return o; }
