// EXPECT: error ambiguous
#include <metal_stdlib>
using namespace metal;
namespace M { namespace K { constant int y = 7; } }
namespace E { typedef struct { int y; } K; }
namespace A { using namespace E; using namespace M; }
kernel void k(device int* out [[buffer(0)]]) { out[0] = int(A::K::y); }
