// EXPECT: valid
// RUN: grid=1
// A function whose name starts like a trait's, overloaded through two using-directives, is still a call: only
// make_scalar, make_signed and make_unsigned followed by "<" are read as a type. On an RTX 3060 out is 6.
#include <metal_stdlib>
namespace a { int make_x(int v) { return v; } }
namespace b { float make_x(float v) { return v * 2.0f; } }
using namespace a;
using namespace b;
kernel void trait_make_overload_not_trait(device uint* out [[buffer(0)]]) { out[0] = uint(make_x(1.5f)) + make_x(3); }
