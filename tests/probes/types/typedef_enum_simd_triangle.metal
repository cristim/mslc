// EXPECT: valid
// DISASM-MATCH: OpEntryPoint Vertex %[0-9]+ "vertexShader"
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 16
// DISASM: ArrayStride 32
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 8
// REFLECT: "metal_index": 0, "descriptor": { "set": 0, "binding": 0 }, "member": 0
// REFLECT: "metal_index": 1, "descriptor": { "set": 0, "binding": 0 }, "member": 1
//
// indium's test/triangle shader with the two things this change does not cover
// taken out: the swizzle store "out.position.xy = ..." and the dereference
// "*viewportSizePointer". Everything else is the sample's, including the header's
// shape: <simd/simd.h>, a typedef'd enum whose enumerators are the buffer indices,
// and an anonymous typedef'd struct of vector_float2 and vector_float4.
#include <metal_stdlib>

using namespace metal;

#include "include/aapl_shader_types.h"

struct RasterizerData
{
    float4 position [[position]];
    float4 color;
};

vertex RasterizerData
vertexShader(uint vertexID [[vertex_id]],
             constant AAPLVertex *vertices [[buffer(AAPLVertexInputIndexVertices)]],
             constant vector_uint2 *viewportSizePointer [[buffer(AAPLVertexInputIndexViewportSize)]])
{
    RasterizerData out;

    float2 pixelSpacePosition = vertices[vertexID].position.xy;
    vector_float2 viewportSize = vector_float2(viewportSizePointer[0]);
    float2 clip = pixelSpacePosition / (viewportSize / 2.0);

    out.position = vector_float4(clip.x, clip.y, 0.0, 1.0);
    out.color = vertices[vertexID].color;

    return out;
}

fragment float4 fragmentShader(RasterizerData in [[stage_in]])
{
    return in.color;
}
