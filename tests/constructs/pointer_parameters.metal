// A pointer parameter, in the spelling a vertex shader usually uses: the
// address space after the const qualifier, reached through "->", with the
// reference form beside it.
//
// The return is a bare float4 rather than a struct, because a returned struct
// whose second member carries no attribute is a gap of its own, reported
// separately; this fixture is about the parameters.
struct Vertex
{
    float4 position;
    float4 color;
};

struct Uniforms
{
    float4 scale;
};

struct FragCoord
{
    float4 position [[position]];
};

vertex float4 vertex_project(const device Vertex *vertices [[buffer(0)]],
                             constant Uniforms *uniforms [[buffer(1)]],
                             uint vid [[vertex_id]])
{
    return uniforms->scale * vertices[vid].color;
}

fragment float4 fragment_flatcolor(FragCoord in [[stage_in]],
                                   constant Uniforms &uniforms [[buffer(0)]])
{
    return in.position * uniforms.scale;
}
