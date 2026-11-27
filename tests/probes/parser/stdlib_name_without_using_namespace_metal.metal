// EXPECT: valid
// DISASM: OpReturnValue %float_7
// DISASM-NO-MATCH: NClamp
// Without "using namespace metal", an unqualified clamp finds only A's, through a
// directive or a using declaration, and Apple calls it.
#include <metal_stdlib>
namespace A { float clamp(float x, float lo, float hi) { return 7.0f; } }
namespace B { float clamp(float x, float lo, float hi) { return 7.0f; } }
using namespace A;
kernel void stdlib_name_without_using_namespace_metal(device int* out [[buffer(0)]])
{
    out[0] = int(clamp(1.0f, 0.0f, 2.0f));
    { using B::clamp; out[1] = int(clamp(1.0f, 0.0f, 2.0f)); }
}
