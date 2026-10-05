#include <metal_stdlib>
using namespace metal;

struct Pair { float a; float b; };

constant uint kThird = 7;

kernel void typedef_aliases(device float *out [[buffer(1)]],
                            constant Pair *pairs [[buffer(0)]],
                            uint i [[thread_position_in_grid]])
{
    const float s = 2.0;
    float3 v = float3(pairs[i].a, pairs[i].b, s);
    float4x4 m = float4x4(float4(1.0), float4(2.0), float4(3.0), float4(4.0));
    uint n = kThird + 7;
    int k = -2 * 3;
    out[i] = v.x + v.y + v.z + m[1].x + float(n) + float(k);
}
