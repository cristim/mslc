// EXPECT: error redefinition of "metal::abs": the standard library defines it with these parameters
// The library's abs, min, max and clamp take unsigned and long integers too (Apple:
// "redefinition of 'abs'").
#include <metal_stdlib>
namespace metal { uint abs(uint x) { return x; } }
kernel void namespace_metal_redefines_unsigned_abs_rejected(device int* out [[buffer(0)]])
{ out[0] = 1; }
