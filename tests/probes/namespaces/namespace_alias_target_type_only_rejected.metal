// EXPECT: error "D::K" is not a namespace the file declares
#include <metal_stdlib>
using namespace metal;
namespace D { enum K { y = 1 }; }
namespace B = D::K;
kernel void k(device int* out [[buffer(0)]]) { out[0] = 1; }
