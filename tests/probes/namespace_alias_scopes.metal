// EXPECT: valid
// DISASM-MATCH: OpIAdd %int %[_0-9a-zA-Z]+ %int_11
// DISASM: OpReturnValue %int_23
// DISASM: OpReturnValue %int_21
// DISASM-NO-MATCH: OpReturnValue %int_11
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; namespace C { constant int y = 23; } }
namespace W { constant int x = 21; }
namespace N {
    namespace B = A;
    namespace A = W;
    int g() { return A::x; }
    int h() { return B::C::y; }
}
namespace { namespace D = A::C; }
kernel void namespace_alias_scopes(device int* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    out[0] = N::g();
    out[1] = N::B::C::y + N::h() - 23;
    out[2] = D::y + 2;
    int B = int(i);
    out[3] = B + N::B::x;
}
