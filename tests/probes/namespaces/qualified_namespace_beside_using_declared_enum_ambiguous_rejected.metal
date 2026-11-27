// EXPECT: error reference to the namespace "B" in "N" is ambiguous
// Apple: "reference to 'B' is ambiguous". N's own namespace B and the enum B
// its using declaration brings in are both found by N::B.
#include <metal_stdlib>
using namespace metal;
namespace E { enum B { x, y }; }
namespace N { namespace B { constant int y = 7; } using E::B; }
kernel void qualified_namespace_beside_using_declared_enum_ambiguous_rejected(device int* out [[buffer(0)]])
{
    out[0] = int(N::B::y);
}
