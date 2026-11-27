// EXPECT: valid
// The sampled type is read whole: texture2d<unsigned int> is texture2d<uint>, an integer image.
// DISASM-MATCH: OpTypeImage %uint 2D 2 0 0 1 Unknown
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<unsigned int> t [[texture(0)]], device uint* o [[buffer(0)]])
{
    o[0] = t.get_width();
}
