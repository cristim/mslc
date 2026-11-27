// EXPECT: error ambiguous
#include <metal_stdlib>
namespace A { constant int x = 11; }
namespace B { namespace M = A; namespace C { enum M { x = 2 }; } using namespace C; }
using namespace B;
kernel void k(device int* out [[buffer(0)]]) { out[0] = M::x; }
