// EXPECT: valid
// DISASM-MATCH: OpEntryPoint Vertex %[0-9]+ "vertex_project" %gl_VertexIndex %[0-9]+ %gl_Position %[0-9]+
// DISASM-MATCH: OpEntryPoint Fragment %[0-9]+ "fragment_flatcolor" %gl_FragCoord %[0-9]+ %[0-9]+
// DISASM: OpDecorate %gl_Position BuiltIn Position
// DISASM: OpDecorate %gl_FragCoord BuiltIn FragCoord
// DISASM: OpMatrixTimesVector %v4float
// DISASM: OpConstantNull
// DISASM-NOT: OpReturnValue
// DISASM-NOT: OpTypePointer Output %v4half
// DISASM-NOT: Flat
//
// indium's test/cube shader, verbatim. The vertex function returns a struct whose
// [[position]] field is the BuiltIn Position and whose color field is an Output
// at Location 0; the fragment function takes the same struct as [[stage_in]],
// so its color is an Input at Location 0, and returns a half4 that is written to
// a float4 Output at Location 0. Rendering this pair on lavapipe and reading the
// pixels back is what shows the two Locations meet; spirv-val alone would accept
// them crossed.
#include <metal_stdlib>

using namespace metal;

struct Vertex
{
    float4 position [[position]];
    float4 color;
};

struct Uniforms
{
    float4x4 modelViewProjectionMatrix;
};


vertex Vertex vertex_project(const device Vertex *vertices [[buffer(0)]],
                             constant Uniforms *uniforms   [[buffer(1)]],
                             uint vid [[vertex_id]])
{
    Vertex vertexOut;
    vertexOut.position = uniforms->modelViewProjectionMatrix * vertices[vid].position;
    vertexOut.color = vertices[vid].color;

    return vertexOut;
}

fragment half4 fragment_flatcolor(Vertex vertexIn [[stage_in]])
{
    return half4(vertexIn.color);
}
