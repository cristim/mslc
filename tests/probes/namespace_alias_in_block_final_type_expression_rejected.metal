// EXPECT: error reference to the namespace "K" in "Root" is ambiguous
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
namespace M { namespace K { constant int x = 13; } }
namespace D { enum K { x = 2 }; }
namespace Root { using namespace M; using namespace D; }
kernel void k(device int* out [[buffer(0)]]) {
    namespace B = Root;
    out[0] = B::K::x;
}
