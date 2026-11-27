// EXPECT: error the call to "A::clamp" also matches the standard library's "clamp"
// Same as the directives case, with using-declarations instead: B::clamp resolves to
// A::clamp through "using A::clamp;", but B's own "using metal::clamp;" also brings
// the library's clamp to B (Apple: "call to 'clamp' is ambiguous"). Pins that the
// written-scope walk, not the resolved name's prefix, decides besideStdlib.
#include <metal_stdlib>
namespace A { float clamp(float x, float lo, float hi) { return 7.0f; } }
namespace B { using A::clamp; using metal::clamp; }
kernel void stdlib_name_qualified_call_via_using_declarations_rejected(device float* out [[buffer(0)]])
{ out[0] = B::clamp(1.5f, 0.0f, 2.0f); }
