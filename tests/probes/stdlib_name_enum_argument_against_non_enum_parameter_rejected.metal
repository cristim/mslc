// EXPECT: error the call to "A::clamp" also matches the standard library's "clamp"
// A::clamp's parameters are plain floats and ints, not the enum E; the enum argument
// converts to int exactly as the library's clamp(int, int, int) would see it (Apple:
// air.clamp.s.i32, verified against xcrun metal -S -emit-llvm), so the library wins.
// Pins that the enum guard added for A::abs(E) (an enum PARAMETER) is keyed on the
// parameter's type, not the argument expression's type: keying it on the argument
// would also exclude this case, where no parameter is an enum and only the argument
// happens to be one, silently calling A's again.
#include <metal_stdlib>
using namespace metal;
enum E { ea, eb };
namespace A { float clamp(float x, int lo, int hi) { return 7.0f; } }
using namespace A;
kernel void stdlib_name_enum_argument_against_non_enum_parameter_rejected(device int* out [[buffer(0)]])
{
    E e = ea;
    out[0] = int(clamp(e, 0, 2));
}
