// EXPECT: error "float2" is a builtin type name
//
// Apple: "redefinition of 'float2' as different kind of symbol".
#include <metal_stdlib>
using namespace metal;
enum One { float2 };
kernel void enum_constant_collides_with_builtin_type_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
