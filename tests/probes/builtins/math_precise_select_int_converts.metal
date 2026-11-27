// EXPECT: valid
// Apple accepts this, converting the int arguments to float (black-box, xcrun metal, macOS 26).
#include <metal_stdlib>
using namespace metal;
kernel void math_precise_select_int_converts(device int* out [[buffer(0)]])
{
    out[0] = precise::select(1, 2, true);
}
