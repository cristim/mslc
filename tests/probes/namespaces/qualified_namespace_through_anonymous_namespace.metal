// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_17[_0-9]* Aligned
// A::K::y with K declared in A's anonymous namespace: a qualified namespace name follows
// A's using directives when A has no K of its own (Apple accepts; #279).
#include <metal_stdlib>
using namespace metal;
namespace A { namespace { namespace K { constant int y = 17; } } }
kernel void qualified_namespace_through_anonymous_namespace(device int* out [[buffer(0)]])
{ out[0] = A::K::y; }
