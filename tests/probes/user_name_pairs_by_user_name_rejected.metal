// EXPECT: error have to be the vertex function's first ones, in order
//
// The fragment's "u" pairs with the vertex function's "t" by their user name, and
// is first in the fragment struct but second in the vertex one; mslc numbers
// those by declaration order, so the pair is reported rather than miscompiled.
struct VOut {
    float4 p [[position]];
    float4 c [[user(color)]];
    float2 t [[user(uv)]];
};

struct FIn {
    float2 u [[user(uv)]];
    float4 c [[user(color)]];
};

vertex VOut user_name_pairs_by_user_name_vertex(uint vid [[vertex_id]])
{
    VOut o;
    o.p = float4(1.0);
    return o;
}

fragment float4 user_name_pairs_by_user_name_rejected(FIn in [[stage_in]])
{
    return in.c + float4(in.u, 0.0, 0.0);
}
