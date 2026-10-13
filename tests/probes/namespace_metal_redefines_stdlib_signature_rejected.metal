// EXPECT: error redefinition of "metal::clamp": the standard library defines it with these parameters
// A definition in namespace metal with one of the library's parameter lists is a
// redefinition (Apple: "redefinition of 'clamp'"), whether or not it is called.
#include <metal_stdlib>
namespace metal { float clamp(float x, float lo, float hi) { return 7.0f; } }
kernel void namespace_metal_redefines_stdlib_signature_rejected(device int* out [[buffer(0)]])
{ out[0] = 1; }
