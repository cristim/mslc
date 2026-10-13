// EXPECT: error the call to "B::clamp" also matches the standard library's "clamp"
// Namespace A's own "using namespace metal;" brings the library's clamp to file
// scope only transitively, through "using namespace A;" (no direct using-namespace-
// metal at file scope). The library's names are still visible beside B::clamp there
// (Apple: the library's clamp wins on these exact float arguments), which a walk
// that checks only the immediate directive's own name would miss.
#include <metal_stdlib>
namespace A { using namespace metal; }
namespace B { float clamp(float x, int lo, int hi) { return 7.0f; } }
using namespace A;
using namespace B;
kernel void stdlib_name_transitive_using_namespace_metal_rejected(device float* out [[buffer(0)]])
{ out[0] = clamp(1.5f, 0.0f, 2.0f); }
