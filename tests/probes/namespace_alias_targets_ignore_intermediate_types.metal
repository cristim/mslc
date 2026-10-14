// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_13[_0-9]* Aligned
#include <metal_stdlib>
using namespace metal;
namespace M { namespace K { namespace L { constant int x = 13; } } }
namespace Root { enum K { y = 1 }; using namespace M; }
namespace B = Root::K::L;
using namespace Root::K::L;
kernel void k(device int* out [[buffer(0)]]) { out[0] = B::x; out[1] = x; }
