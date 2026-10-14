// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_11[_0-9]* Aligned
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_13[_0-9]* Aligned
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_14[_0-9]* Aligned
// DISASM-MATCH: OpISub %int %int_16[_0-9]* %int_16
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_16[_0-9]* Aligned
// DISASM-MATCH: OpIAdd %int %int_17[_0-9]* %int_1
#include <metal_stdlib>
using namespace metal;
namespace KD { namespace KK2 { constant int w = 1; } }
namespace A {
    namespace B { constant int k = 11; constant int k2 = 16; }
    constant int x = 13;
    struct S { int q; int get() const { return q; } };
    enum E { v = 14 };
    namespace { namespace K { constant int y = 17; } }
    using namespace KD;
}
namespace X = A::B;
namespace Y = X;
namespace Z = A::B;
namespace X = A::B;
namespace G = ::A;
namespace KK = A::K;
namespace KW = A::KK2;
kernel void namespace_alias(device int* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    out[0] = X::k;
    out[1] = G::x;
    out[2] = int(G::E::v);
    out[3] = ::Y::k2 - Z::k2 + 15;
    out[4] = Y::k2;
    out[5] = KK::y + KW::w - 1;
    G::S s;
    s.q = int(i);
    out[6] = s.get() + int(G::v);
}
