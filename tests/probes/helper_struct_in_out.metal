// EXPECT: valid
// DISASM: OpFunctionCall
#include <metal_stdlib>
using namespace metal;
struct P { float a; float3 b; };
P swap_scale(P p, float s)
{
    P r;
    r.a = p.b.x * s;
    r.b = float3(p.a * s);
    return r;
}
kernel void k(device P* out [[buffer(0)]], device const P* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{ out[i] = swap_scale(in[i], 2.0f); }
