// EXPECT: error have to be the vertex function's first ones, in order
//
// The fragment's first field agrees with the vertex function's (x at 0); its second
// ("y") is Location 1 in the fragment struct but 2 in the vertex one, past "z".
struct VOut {
    float4 p [[position]];
    float4 x [[user(x)]];
    float4 z [[user(z)]];
    float4 y [[user(y)]];
};

struct FIn {
    float4 x [[user(x)]];
    float4 y [[user(y)]];
};

vertex VOut user_name_second_field_disagrees_vertex(uint vid [[vertex_id]])
{
    VOut o;
    o.p = float4(1.0);
    return o;
}

fragment float4 user_name_second_field_disagrees_rejected(FIn in [[stage_in]])
{
    return in.x + in.y;
}
