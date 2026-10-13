// EXPECT: error the call to "A::clamp" also matches the standard library's "clamp"
// A::clamp's int parameters need a narrowing conversion from these float arguments,
// while metal::clamp's are exact; Apple calls the library's (air.fast_clamp.f32,
// verified against xcrun metal -S -emit-llvm), not A's (7). A::clamp's first
// parameter is a reference, so its own parameter list is not one of the library's
// either; only the call's actual argument types catch this. Pins that a reference
// argument's value, not just its address, is captured for the argument-types match:
// previously no operand was ever recorded for a reference argument, so this case
// silently called A's.
#include <metal_stdlib>
using namespace metal;
namespace A { float clamp(thread float& x, int lo, int hi) { return 7.0f; } }
using namespace A;
kernel void stdlib_name_reference_parameter_exact_arguments_rejected(device float* out [[buffer(0)]])
{
    float v = 1.5f;
    out[0] = clamp(v, 0.0f, 2.0f);
}
