// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_1[_0-9]* Aligned
// DISASM-NO-MATCH: OpStore %[_0-9a-zA-Z]+ %int_7[_0-9]*
#include <metal_stdlib>
using namespace metal;
namespace M { namespace K { constant int y = 7; } }
namespace E { enum T { x, y }; typedef T K; }
namespace A { using E::K; using namespace M; }
kernel void k(device int* out [[buffer(0)]]) { out[0] = int(A::K::y); }
