// EXPECT: error writes "s" as ushort
//
// The vertex writes "s" as ushort and the fragment reads it as uchar: same
// name, different type, no other vertex in this file. mslc reports the
// mismatch instead of silently accepting it at the wrong Location.
struct VOut {
    float4 p [[position]];
    ushort s;
};

struct FIn {
    float4 p [[position]];
    uchar s;
};

vertex VOut stage_interface_type_mismatch_vertex(uint vid [[vertex_id]])
{
    VOut o;
    o.p = float4(1.0);
    o.s = 0;
    return o;
}

fragment float4 stage_interface_type_mismatch_rejected(FIn in [[stage_in]])
{
    return float4(float(in.s));
}
