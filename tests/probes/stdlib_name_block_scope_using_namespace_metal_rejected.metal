// EXPECT: error the call to "A::clamp" also matches the standard library's "clamp"
// "using namespace metal;" written inside the function body (not at file scope) still
// brings the library's clamp beside A::clamp(float, float, float) for the call below
// (Apple: "call to 'clamp' is ambiguous"). Pins the _blocks walk in unqualifiedFound:
// deleting its stdlib-reached tracking does not fail any other probe.
#include <metal_stdlib>
namespace A { float clamp(float x, float lo, float hi) { return 7.0f; } }
using namespace A;
kernel void stdlib_name_block_scope_using_namespace_metal_rejected(device int* out [[buffer(0)]])
{
    using namespace metal;
    out[0] = int(clamp(1.0f, 0.0f, 2.0f));
}
