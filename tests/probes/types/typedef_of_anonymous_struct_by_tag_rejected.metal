// EXPECT: error cannot be referenced with a struct specifier
//
// Apple: "typedef 'Thing' cannot be referenced with a struct specifier".
#include <metal_stdlib>
using namespace metal;
typedef struct { float a; } Thing;
typedef struct Thing Other;
kernel void typedef_of_anonymous_struct_by_tag_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
