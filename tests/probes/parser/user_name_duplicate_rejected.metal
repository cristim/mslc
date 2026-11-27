// EXPECT: error duplicated user-defined name "a"
struct VOut {
    float4 p [[position]];
    float2 t [[user(a)]];
    float4 c [[user(a)]];
};

vertex VOut user_name_duplicate_rejected(uint vid [[vertex_id]])
{
    VOut o;
    o.p = float4(1.0);
    return o;
}
