// EXPECT: valid
// A const reference parameter is not one of the library's value parameter lists, so this
// clamp overloads the library's rather than redefining it (Apple accepts).
#include <metal_stdlib>
namespace metal { float clamp(const thread float& x, float lo, float hi) { return 7.0f; } }
kernel void namespace_metal_const_reference_overload_accepted(device float* out [[buffer(0)]])
{ out[0] = 1.0f; }
