// EXPECT: valid
// A bool is a number to write: Apple converts it to the element type and broadcasts it.
// DISASM: OpImageWrite
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float, access::write> dst, uint3 g [[thread_position_in_grid]]) { dst.write(g.x > 1, g.xy); }
