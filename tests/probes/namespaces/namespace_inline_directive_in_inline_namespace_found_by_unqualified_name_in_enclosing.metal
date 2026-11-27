// EXPECT: valid
// DISASM: OpFunctionCall
// A using directive in an inline namespace of A nominates X, so the unqualified Q::v inside
// A's function finds X::Q (Apple stores 7).
#include <metal_stdlib>
using namespace metal;
namespace X { namespace Q { constant float v = 7; } } namespace A { inline namespace I { using namespace X; } float f() { return Q::v; } } kernel void kern(device float* out [[buffer(0)]]) { out[0] = A::f(); }
