// EXPECT: error reference to the namespace "B" in "N" is ambiguous
// Apple: "reference to 'B' is ambiguous", for a struct as for an enum.
#include <metal_stdlib>
using namespace metal;
namespace E { struct B { int y; }; }
namespace N { namespace B { constant int y = 7; } using E::B; }
kernel void qualified_namespace_beside_using_declared_struct_ambiguous_rejected(device int* out [[buffer(0)]])
{
    out[0] = N::B::y;
}
