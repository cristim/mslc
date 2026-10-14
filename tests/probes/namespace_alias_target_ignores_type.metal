// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_7[_0-9]* Aligned
#include <metal_stdlib>
using namespace metal;
namespace D { enum K { y = 1 }; }
namespace M { namespace K { constant int y = 7; } }
namespace Q = M;
namespace A { using namespace D; using namespace Q; }
namespace B = A::K;
using namespace A::K;
kernel void k(device int* out [[buffer(0)]]) { out[0] = B::y; out[1] = y; }
