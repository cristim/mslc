// EXPECT: error is not a parameter, local, constant or builtin
#include <metal_stdlib>
using namespace metal;
namespace M { namespace K { constant int y = 7; } }
namespace A { typedef struct { int y; } K; using namespace M; }
kernel void k(device int* out [[buffer(0)]]) { out[0] = int(A::K::y); }
