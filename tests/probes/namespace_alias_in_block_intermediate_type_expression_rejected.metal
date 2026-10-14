// EXPECT: error no member named "L" in the enum "Root::K"
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
namespace M { namespace K { namespace L { constant int x = 13; } } }
namespace Root { enum K { y = 1 }; using namespace M; }
kernel void k(device int* out [[buffer(0)]]) {
    namespace B = Root;
    out[0] = B::K::L::x;
}
