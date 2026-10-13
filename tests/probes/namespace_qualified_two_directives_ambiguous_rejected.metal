// EXPECT: error reference to "A::v" is ambiguous
//
// Guard: A declares no own v, so [namespace.qual] falls through to the union
// of both directives' v, which is still ambiguous. No existing probe covers
// a qualified name reaching two directives (the existing ambiguity probes
// are all unqualified).
#include <metal_stdlib>
using namespace metal;
namespace B { constant int v = 22; }
namespace C { constant int v = 33; }
namespace A { using namespace B; using namespace C; }
kernel void namespace_qualified_two_directives_ambiguous_rejected(device int *o [[buffer(0)]])
{
    o[0] = A::v;
}
