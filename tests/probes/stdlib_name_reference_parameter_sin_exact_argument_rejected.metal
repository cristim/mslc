// EXPECT: error the call to "A::sin" also matches the standard library's "sin"
// Same as the clamp case, for a one-argument builtin: A::sin(thread float&) is not
// one of the library's parameter lists, but the call's float argument is exactly
// metal::sin's (Apple: ambiguous), which this now rejects instead of silently
// calling A's.
#include <metal_stdlib>
using namespace metal;
namespace A { float sin(thread float& x) { return 7.0f; } }
using namespace A;
kernel void stdlib_name_reference_parameter_sin_exact_argument_rejected(device float* out [[buffer(0)]])
{
    float v = 1.5f;
    out[0] = sin(v);
}
