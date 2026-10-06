// EXPECT: valid
// DISASM-MATCH: OpDecorate %[0-9]+ Centroid
// DISASM-NOT: SampleRateShading
//
// Only sample-rate interpolation needs the SampleRateShading capability (and the
// sampleRateShading device feature); centroid does not.
struct C {
    float4 p [[position]];
    float a [[centroid_perspective]];
    float2 b [[centroid_no_perspective]];
};

vertex C interpolation_centroid_vs(uint vid [[vertex_id]])
{
    C o;
    o.p = float4(0.0);
    o.a = 1.0;
    o.b = float2(1.0);
    return o;
}

fragment float4 interpolation_centroid_fs(C in [[stage_in]])
{
    return float4(in.a, in.b, 1.0);
}
