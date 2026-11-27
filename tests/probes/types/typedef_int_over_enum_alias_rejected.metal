// EXPECT: error typedef redefinition with different types ('int' vs 'Mode')
//
// Same shape as typedef_int_over_enum_tag_rejected.metal, but over an
// enum-aliasing typedef rather than the enum's own tag name.
#include <metal_stdlib>
using namespace metal;
enum Mode { A };
typedef Mode Alias;
typedef int Alias;
kernel void typedef_int_over_enum_alias_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
