// EXPECT: error "metal::nope" is not a namespace the file declares
#include <metal_stdlib>
using namespace metal::nope;
kernel void k(device int* o [[buffer(0)]]) { o[0] = 1; }
