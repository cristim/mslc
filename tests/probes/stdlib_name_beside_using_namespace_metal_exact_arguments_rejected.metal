// EXPECT: error the call to "A::clamp" also matches the standard library's "clamp"
// The arguments are exactly metal::clamp(float, float, float), which Apple calls (1) rather
// than A::clamp(float, int, int) (7), whose parameters are none of the library's. mslc cannot
// choose between them, so it rejects the call rather than call A::clamp, as it did before.
#include <metal_stdlib>
using namespace metal;
namespace A { int clamp(float x, int lo, int hi) { return 7; } }
using namespace A;
kernel void stdlib_name_beside_using_namespace_metal_exact_arguments_rejected(device int* out [[buffer(0)]])
{ out[0] = int(clamp(1.5f, 0.0f, 2.0f)); }
