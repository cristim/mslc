// EXPECT: error the call to "A::clamp" also matches the standard library's "clamp"
// A::clamp(float, int, int) is not itself one of the library's parameter lists, but
// "using metal::clamp;" inside namespace A brings the library's clamp there too, so a
// qualified A::clamp(...) finds both (Apple: the library's clamp wins on these exact
// float arguments, matching the file-scope "using metal::name;" case one level in).
#include <metal_stdlib>
using namespace metal;
namespace A {
    using metal::clamp;
    float clamp(float x, int lo, int hi) { return 7.0f; }
}
kernel void stdlib_name_qualified_call_into_namespace_with_using_declaration_rejected(device float* out [[buffer(0)]])
{ out[0] = A::clamp(1.5f, 0.0f, 2.0f); }
