// EXPECT: error needs an identifier
struct VOut {
    float4 p [[position]];
    float2 t [[user(1)]];
};

vertex VOut user_name_integer_rejected(uint vid [[vertex_id]])
{
    VOut o;
    o.p = float4(1.0);
    return o;
}
