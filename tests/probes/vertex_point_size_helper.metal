// EXPECT: valid
// DISASM: BuiltIn PointSize
//
// A helper function fills the struct, not the entry point directly.
struct VertexOut {
    float4 p [[position]];
    float size [[point_size]];
};

VertexOut makeVertexOut(float4 position, float size)
{
    VertexOut o;
    o.p = position;
    o.size = size;
    return o;
}

vertex VertexOut vertex_point_size_helper(uint vid [[vertex_id]])
{
    return makeVertexOut(float4(0.0, 0.0, 0.0, 1.0), 6.0);
}
