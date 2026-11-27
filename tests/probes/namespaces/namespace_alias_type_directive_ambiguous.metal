// EXPECT: error ambiguous
#include <metal_stdlib>
namespace A { constant int x = 11; }
namespace M = A;
namespace E { enum M { x = 2 }; }
kernel void k(device int* out [[buffer(0)]]) { using namespace E; out[0] = M::x; }
