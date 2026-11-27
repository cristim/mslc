// EXPECT: error the call to "A::clamp" also matches the standard library's "clamp"
// Inside namespace metal the library's clamp is the namespace's own and hides A::clamp from the
// file's directive; Apple calls the library's (air.fast_clamp.f32). mslc rejects rather than
// call A::clamp (7), as it did before.
#include <metal_stdlib>
namespace A { float clamp(float x, int lo, int hi) { return 7.0f; } }
using namespace A;
namespace metal { float g() { return clamp(1.5f, 0.0f, 2.0f); } }
kernel void namespace_metal_code_calls_outer_function_rejected(device float* out [[buffer(0)]])
{ out[0] = metal::g(); }
