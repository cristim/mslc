// EXPECT: error ambiguous
#include <metal_stdlib>
namespace A { constant int x = 11; }
namespace B { namespace M = A; }
namespace C { namespace M { constant int x = 3; } }
kernel void k(device int* out [[buffer(0)]]) { using namespace B; using namespace C; out[0] = M::x; }
