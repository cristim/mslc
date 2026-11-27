// EXPECT: error the call to "A::clamp" also matches the standard library's "clamp"
// With both "using namespace metal" and "using namespace A" at file scope, A::clamp and
// metal::clamp(float, float, float) have one parameter list, so a call is ambiguous unless its
// arguments exactly match another library overload (clamp(1, 0, 2) is the library's int
// clamp); here they match neither exactly (Apple: "call to 'clamp' is ambiguous").
#include <metal_stdlib>
using namespace metal;
namespace A { float clamp(float x, float lo, float hi) { return 7.0f; } }
using namespace A;
kernel void stdlib_name_beside_using_namespace_metal_same_signature_rejected(device int* out [[buffer(0)]])
{ out[0] = int(clamp(1.0f, 0, 2)); }
