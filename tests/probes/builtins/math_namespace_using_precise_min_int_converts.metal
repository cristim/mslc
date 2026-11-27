// EXPECT: valid
// Apple accepts this, converting the int arguments to float (black-box, xcrun metal, macOS 26).
#include <metal_stdlib>
namespace A { using metal::precise::min; }
kernel void math_namespace_using_precise_min_int_converts(device int* out [[buffer(0)]])
{
    out[0] = A::min(1, 2);
}
