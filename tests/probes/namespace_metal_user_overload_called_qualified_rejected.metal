// EXPECT: error the call to "metal::clamp" also matches the standard library's "clamp"
// A clamp the file adds to namespace metal sits beside the library's own: metal::clamp with
// float arguments is the library's (Apple calls air.fast_clamp.f32), not the file's (7).
#include <metal_stdlib>
namespace metal { float clamp(float x, int lo, float hi) { return 7.0f; } }
kernel void namespace_metal_user_overload_called_qualified_rejected(device float* out [[buffer(0)]])
{ out[0] = metal::clamp(1.5f, 0.0f, 2.0f); }
