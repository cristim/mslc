// EXPECT: error the call to "A::clamp" also matches the standard library's "clamp"
// "using metal::clamp;" brings the library's clamp to file scope, where "using namespace A"
// also puts A::clamp(float, int, int). The arguments are exactly the library's float form,
// which Apple calls; mslc rejects rather than call A::clamp (7), as it did before.
#include <metal_stdlib>
namespace A { float clamp(float x, int lo, int hi) { return 7.0f; } }
using metal::clamp;
using namespace A;
kernel void stdlib_name_using_declaration_of_library_beside_directive_rejected(device float* out [[buffer(0)]])
{ out[0] = clamp(1.5f, 0.0f, 2.0f); }
