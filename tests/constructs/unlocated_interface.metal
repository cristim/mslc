// A struct crossing a stage boundary with a member that names no attribute,
// which Metal gives the next free location rather than rejecting. A vertex
// output and the fragment input that receives it are the same declaration
// read from two sides, so both count the unlocated members the same way.
//
// The float4 and half4 mix is here because that is what a shader's colour is:
// "half4(aFloat4)" narrows each component rather than reinterpreting the bits.
struct Vertex
{
    float4 position [[position]];
    float4 color;
    float3 normal;
};

vertex Vertex vertex_project(const device Vertex *vertices [[buffer(0)]],
                             constant float4 *scale [[buffer(1)]],
                             uint vid [[vertex_id]])
{
    Vertex out;
    out.position = vertices[vid].position;
    out.color = half4(vertices[vid].color * scale[0]);
    out.normal = vertices[vid].normal;

    return out;
}

fragment half4 fragment_flatcolor(Vertex vertexIn [[stage_in]])
{
    return half4(vertexIn.color) + half4(vertexIn.normal, 1.0);
}
