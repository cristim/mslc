// EXPECT: error the call to "A::abs" also matches the standard library's "abs"
// The library has abs(ushort), an exact match that Apple calls (air.abs.u.i16), not A::abs(bool).
#include <metal_stdlib>
using namespace metal;
namespace A { float abs(bool b) { return 7.0f; } }
using namespace A;
kernel void stdlib_name_unsigned_argument_beside_using_namespace_metal_rejected(device float* out [[buffer(0)]])
{ ushort u = 3; out[0] = abs(u); }
