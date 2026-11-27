// EXPECT: valid
// DISASM: BuiltIn PointSize
// DISASM: BuiltIn Position
//
// The point_size field comes before position in declaration order.
struct VertexOut {
    float size [[point_size]];
    float4 p [[position]];
};

vertex VertexOut vertex_point_size_first(uint vid [[vertex_id]])
{
    VertexOut o;
    o.size = 4.0;
    o.p = float4(0.0, 0.0, 0.0, 1.0);
    return o;
}
