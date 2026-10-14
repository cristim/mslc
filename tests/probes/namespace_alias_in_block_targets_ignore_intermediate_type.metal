// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_13[_0-9]* Aligned
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
namespace M { namespace K { namespace L { constant int x = 13; } } }
namespace Root { enum K { y = 1 }; using namespace M; }
kernel void k(device int* out [[buffer(0)]]) {
    namespace B = Root;
    namespace C = B::K::L;
    using namespace B::K::L;
    out[0] = C::x; out[1] = x;
}
