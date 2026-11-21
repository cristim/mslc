// EXPECT: error reference to the namespace "B" in "N" is ambiguous
// Apple: "reference to 'B' is ambiguous", for a typedef as for an enum.
#include <metal_stdlib>
using namespace metal;
namespace E { typedef int B; }
namespace N { namespace B { constant int y = 7; } using E::B; }
kernel void qualified_namespace_beside_using_declared_typedef_ambiguous_rejected(device int* out [[buffer(0)]])
{
    out[0] = N::B::y;
}
