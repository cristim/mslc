// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_1[_0-9]* Aligned
// DISASM-NO-MATCH: OpStore %[_0-9a-zA-Z]+ %int_7[_0-9]* Aligned
// A's own enum K is its own answer to "K", even though it is not a namespace: the walk
// must not keep searching A's directive (to M, whose namespace K has a "y" of 7) past it.
// A::K::y names the enum's enumerator (1), through resolveName's enum-member fallback.
#include <metal_stdlib>
using namespace metal;
namespace M { namespace K { constant int y = 7; } }
namespace A { using namespace M; enum K { x, y }; }
kernel void qualified_namespace_own_type_beats_directive(device int* out [[buffer(0)]])
{ out[0] = int(A::K::y); }
