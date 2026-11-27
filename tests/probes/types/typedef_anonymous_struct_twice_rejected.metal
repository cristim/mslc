// EXPECT: error redefinition of "Thing"
//
// Two anonymous structs with one typedef name are two types, which Apple rejects as a typedef redefinition.
#include <metal_stdlib>
using namespace metal;
typedef struct { float a; } Thing;
typedef struct { float b; } Thing;
kernel void typedef_anonymous_struct_twice_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
