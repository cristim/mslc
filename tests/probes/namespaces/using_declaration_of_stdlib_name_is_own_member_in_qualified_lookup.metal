// EXPECT: valid
// Apple calls the library's clamp: N's using declaration of it is N's own member,
// so N::clamp never searches N's using directive of A (which has its own clamp).
// DISASM-MATCH: OpExtInst %float %[0-9]+ NClamp %
// DISASM-NO-MATCH: OpReturnValue %float_99
#include <metal_stdlib>
using namespace metal;
namespace A { float clamp(float, float, float) { return 99.0f; } }
namespace N { using metal::clamp; using namespace A; }
kernel void using_declaration_of_stdlib_name_is_own_member_in_qualified_lookup(device float* out [[buffer(0)]])
{
    out[0] = N::clamp(5.0f, 0.0f, 1.0f);
}
