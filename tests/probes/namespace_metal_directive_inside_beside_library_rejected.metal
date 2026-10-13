// EXPECT: error the call to "metal::A::clamp" also matches the standard library's "clamp"
// Inside namespace metal the library's names are the namespace's own, so a directive there
// puts A::clamp beside them (Apple calls the library's clamp for these float arguments).
#include <metal_stdlib>
namespace metal {
    namespace A { float clamp(float x, int lo, int hi) { return 7.0f; } }
    using namespace A;
    float g() { return clamp(1.5f, 0.0f, 2.0f); }
}
kernel void namespace_metal_directive_inside_beside_library_rejected(device float* out [[buffer(0)]])
{ out[0] = metal::g(); }
