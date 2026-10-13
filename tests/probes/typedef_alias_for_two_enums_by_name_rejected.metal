// EXPECT: error typedef redefinition with different types ('Other' vs 'Mode')
//
// Two different enums colliding on one alias name through the generic typedef
// path (`typedef Mode Alias; typedef Other Alias;`), distinct from
// typedef_enum_alias_for_two_enums_rejected.metal, which goes through the
// `isEnum` branch (`typedef enum E Alias` form) rather than this one.
#include <metal_stdlib>
using namespace metal;
enum Mode { A };
enum Other { B };
typedef Mode Alias;
typedef Other Alias;
kernel void typedef_alias_for_two_enums_by_name_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
