// EXPECT: valid
// DISASM: NClamp
// DISASM-NO-MATCH: OpFunctionCall
// N::clamp names the library's clamp when the lookup in N reaches the library's names and
// nothing the file declares: "using metal::clamp;" in N, in a namespace N's directive
// nominates, or "using namespace metal;" in N (Apple accepts all three).
#include <metal_stdlib>
namespace N { using metal::clamp; }
namespace D { using metal::clamp; }
namespace P { using namespace D; }
namespace Q { using namespace metal; }
kernel void qualified_using_declared_library_name(device float* out [[buffer(0)]])
{ out[0] = N::clamp(5.0f, 0.0f, 1.0f) + P::clamp(5.0f, 0.0f, 1.0f) + Q::clamp(5.0f, 0.0f, 1.0f); }
