// EXPECT: valid
// DISASM: OpCompositeConstruct %v4float
//
// A scalar returned from an entry point fills every component of the vector
// return type, as it does from a helper and in an initialiser.
#include <metal_stdlib>
using namespace metal;

namespace n { void g() {} }
void h() {}

fragment float4 entry_return_scalar_to_vector(float4 p [[position]])
{
    n::g();
    h();
    h();
    if (p.x > 1.0f) {
        return 1;
    }
    return 0;
}
