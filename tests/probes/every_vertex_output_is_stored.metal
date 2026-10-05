// EXPECT: valid
// DISASM: OpFConvert %float
// DISASM: OpFConvert %v2float
// DISASM: OpFConvert %v3float
// DISASM: OpStore %gl_Position
//
// Every field of a returned struct is stored to its Output. A half crosses as a
// float, and the widening is emitted only as the value of that store, so one
// OpFConvert per width shows each of the three non-position fields was stored,
// the last one included.
struct Out {
    float4 p [[position]];
    half a;
    half2 b;
    half3 c;
};

vertex Out every_vertex_output_is_stored(uint vid [[vertex_id]])
{
    Out o;
    o.p = float4(1.0);
    return o;
}
