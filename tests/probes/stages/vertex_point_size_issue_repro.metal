// EXPECT: valid
// DISASM: BuiltIn PointSize
//
// Issue #184's reproducer: a vertex function returning a struct built from a
// constant, which used to reject with "unsupported attribute \"point_size\"".
struct VertexOut {
    float4 position [[position]];
    float pointSize [[point_size]];
};

vertex VertexOut vertex_point_size_issue_repro(uint vid [[vertex_id]])
{
    constexpr float4 pos = float4(0.0, 0.0, 0.0, 1.0);
    VertexOut o;
    o.position = pos;
    o.pointSize = 10.0;
    return o;
}
