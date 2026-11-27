// EXPECT: error also matches the standard library's "precise::min"
#include <metal_stdlib>
namespace metal { namespace precise { int min(int a, int b) { return a; } } }
kernel void math_namespace_metal_precise_beside_stdlib_rejected(device int* out [[buffer(0)]])
{
    out[0] = metal::precise::min(1.0f, 2.0f);
}
