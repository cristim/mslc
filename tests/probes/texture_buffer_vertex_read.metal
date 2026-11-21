// EXPECT: valid
// DISASM: OpTypeImage %float Buffer 2 0 0 1 Unknown
#include <metal_stdlib>
using namespace metal;
vertex float4 v(texture_buffer<float, access::read> r [[texture(0)]], uint i [[vertex_id]]) { return r.read(i); }
