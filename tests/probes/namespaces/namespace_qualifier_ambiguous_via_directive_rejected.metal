// EXPECT: error reference to "B2" is ambiguous
#include <metal_stdlib>
namespace A { namespace B2 { constant int g = 15; } }
namespace B2 { constant int g = 17; }
using namespace A;
kernel void k(device int* o [[buffer(0)]]) { o[0] = B2::g; }
