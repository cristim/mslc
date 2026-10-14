// EXPECT: valid
// DISASM: NClamp
// DISASM-NO-MATCH: OpFunctionCall
// A::K reaches Z::K through A's directive (#279), and K's "using metal::clamp;" makes
// A::K::clamp the library's clamp (Apple accepts).
#include <metal_stdlib>
namespace Z { namespace K { using metal::clamp; } }
namespace A { using namespace Z; }
kernel void qualified_namespace_through_directive_library_member(device float* out [[buffer(0)]])
{ out[0] = A::K::clamp(5.0f, 0.0f, 1.0f); }
