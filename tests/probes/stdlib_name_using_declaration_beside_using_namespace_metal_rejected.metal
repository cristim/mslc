// EXPECT: error the call to "A::clamp" also matches the standard library's "clamp"
// A file-scope using declaration and the library's names meet at the same level
// (Apple: "call to 'clamp' is ambiguous").
#include <metal_stdlib>
using namespace metal;
namespace A { float clamp(float x, float lo, float hi) { return 7.0f; } }
using A::clamp;
kernel void stdlib_name_using_declaration_beside_using_namespace_metal_rejected(device int* out [[buffer(0)]])
{ out[0] = int(clamp(1.0f, 0.0f, 2.0f)); }
