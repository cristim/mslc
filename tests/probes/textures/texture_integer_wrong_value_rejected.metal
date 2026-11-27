// EXPECT: error texture write takes exactly an int4 or a number
// Apple: no matching member function for write (uint4 into a texture2d<int>).
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<int, access::write> a [[texture(0)]], uint2 t [[thread_position_in_grid]]) {
  a.write(uint4(1), t);
}
