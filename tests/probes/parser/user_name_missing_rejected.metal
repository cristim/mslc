// EXPECT: error needs a name
struct VOut {
    float4 p [[position]];
    float2 t [[user]];
};

vertex VOut user_name_missing_rejected(uint vid [[vertex_id]])
{
    VOut o;
    o.p = float4(1.0);
    return o;
}
