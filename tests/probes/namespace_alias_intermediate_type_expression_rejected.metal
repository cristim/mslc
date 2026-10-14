// EXPECT: error no member named "L" in the enum "Root::K"
#include <metal_stdlib>
using namespace metal;
namespace M { namespace K { namespace L { constant int x = 13; } } }
namespace Root { enum K { y = 1 }; using namespace M; }
namespace Q = Root;
kernel void k(device int* out [[buffer(0)]]) { out[0] = Q::K::L::x; }
