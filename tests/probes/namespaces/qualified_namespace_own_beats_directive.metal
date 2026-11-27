// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_19[_0-9]* Aligned
// DISASM-NO-MATCH: OpStore %[_0-9a-zA-Z]+ %int_2[_0-9]* Aligned
// A's own K wins over the K its directive reaches (Apple accepts and reads 19).
#include <metal_stdlib>
using namespace metal;
namespace Z { namespace K { constant int y = 2; } }
namespace A { namespace K { constant int y = 19; } using namespace Z; }
kernel void qualified_namespace_own_beats_directive(device int* out [[buffer(0)]])
{ out[0] = A::K::y; }
