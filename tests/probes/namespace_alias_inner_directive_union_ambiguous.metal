// EXPECT: error ambiguous
#include <metal_stdlib>
namespace A { constant int x = 11; }
namespace Host { namespace M = A; namespace E { enum M { x = 2 }; }
using namespace E;
kernel void k(device int* out [[buffer(0)]]) { out[0] = M::x; }
}
