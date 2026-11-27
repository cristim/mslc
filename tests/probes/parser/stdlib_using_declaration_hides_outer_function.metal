// EXPECT: valid
// DISASM: NClamp
// DISASM-NO-MATCH: OpReturnValue %float_7
// "using metal::clamp;" in N makes the library's clamp N's own, which hides A::clamp from
// the file's directive: Apple calls the library's clamp (air.fast_clamp.f32), not A's (7).
#include <metal_stdlib>
namespace A { float clamp(float x, int lo, int hi) { return 7.0f; } }
using namespace A;
namespace N { using metal::clamp; float g() { return clamp(1.5f, 0.0f, 2.0f); } }
kernel void stdlib_using_declaration_hides_outer_function(device float* out [[buffer(0)]])
{ out[0] = N::g(); }
