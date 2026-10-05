#include <metal_stdlib>
using namespace metal;

struct AAPLVertex
{
    float2 position;
    float4 color;
};

struct RasterizerData
{
    float4 position [[position]];
    float4 color;
};

vertex RasterizerData
vertexShader(uint vertexID [[vertex_id]],
             constant AAPLVertex *vertices [[buffer(0)]],
             constant uint2 *viewportSizePointer [[buffer(1)]])
{
    RasterizerData out;
    float2 viewportSize = float2(viewportSizePointer[0]);
    float2 clip = vertices[vertexID].position / (viewportSize / 2.0);
    out.position = float4(clip.x, clip.y, 0.0, 1.0);
    out.color = vertices[vertexID].color;
    return out;
}
