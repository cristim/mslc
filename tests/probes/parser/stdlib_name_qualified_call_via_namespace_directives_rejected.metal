// EXPECT: error the call to "A::clamp" also matches the standard library's "clamp"
// B::clamp resolves to A::clamp through B's own "using namespace A;", but B also has
// "using namespace metal;": the qualified lookup is at B, not at A (where the member
// was found), so besideStdlib has to be computed from the WRITTEN scope "B", not from
// the resolved name's "A::" prefix (Apple: "call to 'clamp' is ambiguous"; the earlier
// owner-based check missed this, since the resolved owner is A, which has no route to
// metal of its own).
#include <metal_stdlib>
namespace A { float clamp(float x, float lo, float hi) { return 7.0f; } }
namespace B { using namespace A; using namespace metal; }
kernel void stdlib_name_qualified_call_via_namespace_directives_rejected(device float* out [[buffer(0)]])
{ out[0] = B::clamp(1.5f, 0.0f, 2.0f); }
