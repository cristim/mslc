// EXPECT: error reference to the namespace "K" in "A" is ambiguous: it names D::K and M::K
#include <metal_stdlib>
using namespace metal;
namespace D { enum K { y = 1 }; }
namespace M { namespace K { constant int y = 7; } }
namespace Q = M;
namespace A { using namespace D; using namespace Q; }
kernel void k(device int* out [[buffer(0)]]) { out[0] = A::K::y; }
