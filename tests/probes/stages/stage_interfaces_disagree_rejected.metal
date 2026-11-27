// EXPECT: error have to be the vertex function's first ones, in order
//
// mslc numbers Locations by declaration order, where Apple pairs a vertex
// output with a fragment input by name and type. Here the fragment's struct
// lists the two colours the other way round, so numbering by order would hand
// "b" the vertex function's "a"; the module is reported instead.
struct VOut {
    float4 p [[position]];
    float4 a;
    float4 b;
};

struct FIn {
    float4 p [[position]];
    float4 b;
    float4 a;
};

vertex VOut stage_interfaces_disagree_vertex(uint vid [[vertex_id]])
{
    VOut o;
    o.p = float4(1.0);
    return o;
}

fragment float4 stage_interfaces_disagree_rejected(FIn in [[stage_in]])
{
    return in.a;
}
