// EXPECT: error redefinition of "metal": it is the standard library's namespace
// Apple: "redefinition of 'metal'".
#include <metal_stdlib>
namespace A { constant int x = 11; }
namespace metal = A;
kernel void namespace_alias_named_metal_rejected(device int* out [[buffer(0)]])
{ out[0] = metal::x; }
