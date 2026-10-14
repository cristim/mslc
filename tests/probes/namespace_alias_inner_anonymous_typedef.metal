// EXPECT: error ambiguous
#include <metal_stdlib>
namespace A { constant int x = 11; }
namespace M = A;
namespace E { typedef struct { int x; } M; }
kernel void k(device int* out [[buffer(0)]]) { using namespace E; out[0] = M::x; }
