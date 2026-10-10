// EXPECT: valid
// DISASM: BuiltIn PointSize
//
// The point_size field's type is a typedef of float, not the spelling "float"
// itself; the alias has to resolve to the same scalar check.
typedef float F;

struct VertexOut {
    float4 p [[position]];
    F size [[point_size]];
};

vertex VertexOut vertex_point_size_alias(uint vid [[vertex_id]])
{
    VertexOut o;
    o.p = float4(0.0, 0.0, 0.0, 1.0);
    o.size = 2.0;
    return o;
}
