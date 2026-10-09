// EXPECT: valid
// DISASM: BuiltIn PointSize
// DISASM: BuiltIn Position
// DISASM: Location 0
//
// A point_size field consumes no Location, so the user field after it still
// gets Location 0 (regression for stageLocations skipping point_size).
struct VertexOut {
    float4 p [[position]];
    float size [[point_size]];
    float4 color;
};

vertex VertexOut vertex_point_size(uint vid [[vertex_id]])
{
    VertexOut o;
    o.p = float4(0.0, 0.0, 0.0, 1.0);
    o.size = 8.0;
    o.color = float4(1.0);
    return o;
}
