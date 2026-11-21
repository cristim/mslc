// EXPECT: error which is not a texture2d<float, access::read_write>
#include <metal_stdlib>
using namespace metal;
void bump(texture2d<float, access::read_write> t, uint2 p) { t.write(t.read(p), p); }
kernel void hit(texture2d<float, access::write> a [[texture(0)]], uint2 g [[thread_position_in_grid]]) {
  bump(a, g);
}
