// EXPECT: error texture fence takes no arguments
// Apple: no matching member function for call to 'fence'.
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float, access::read_write> a [[texture(0)]]) { a.fence(0); }
