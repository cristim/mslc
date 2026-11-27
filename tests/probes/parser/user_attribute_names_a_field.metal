// EXPECT: valid
// DISASM-MATCH: OpDecorate %[A-Za-z0-9_]+ Location 0
// DISASM-MATCH: OpDecorate %[A-Za-z0-9_]+ Location 1
//
// [[user(name)]] takes an identifier, and Apple pairs a vertex output with a
// fragment input by it. Names that are not locnN keep declaration order. Also
// accepted, as Apple accepts them: user on [[position]], beside an interpolation
// attribute, on an integer field and next to a field with no user name.
struct VOut {
    float4 p [[position, user(pos)]];
    float2 t [[user(texturecoord)]];
    int i [[user(index), flat]];
    float4 c;
};

struct FIn {
    float4 p [[position]];
    float2 t [[user(texturecoord)]];
    int i [[user(index), flat]];
    float4 c;
};

vertex VOut user_attribute_names_a_field_vertex(uint vid [[vertex_id]])
{
    VOut o;
    o.p = float4(1.0);
    o.t = float2(1.0);
    o.i = 2;
    o.c = float4(1.0);
    return o;
}

fragment float4 user_attribute_names_a_field(FIn in [[stage_in]])
{
    return in.c + float4(in.t, float(in.i), 0.0);
}
