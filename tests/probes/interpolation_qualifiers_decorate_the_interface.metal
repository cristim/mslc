// EXPECT: valid
// DISASM-MATCH: Location 1[^%]*%[0-9]+ Flat
// DISASM-NO-MATCH: Location 2[^%]*%[0-9]+ (Flat|NoPerspective|Centroid|Sample)
// DISASM-MATCH: Location 3[^%]*%[0-9]+ NoPerspective
// DISASM-NO-MATCH: Location 3[^%]*%[0-9]+ NoPerspective[^%]*%[0-9]+ (Centroid|Sample)
// DISASM-MATCH: Location 4[^%]*%[0-9]+ Centroid
// DISASM-NO-MATCH: Location 4[^%]*%[0-9]+ Centroid[^%]*%[0-9]+ NoPerspective
// DISASM-MATCH: Location 5[^%]*%[0-9]+ Centroid[^%]*%[0-9]+ NoPerspective
// DISASM-MATCH: Location 6[^%]*%[0-9]+ Sample
// DISASM-NO-MATCH: Location 6[^%]*%[0-9]+ Sample[^%]*%[0-9]+ NoPerspective
// DISASM-MATCH: Location 7[^%]*%[0-9]+ Sample[^%]*%[0-9]+ NoPerspective
// DISASM: OpCapability SampleRateShading
// DISASM-MATCH: OpDecorate %[0-9]+ Flat.*OpDecorate %[0-9]+ Flat
// DISASM-MATCH: OpDecorate %[0-9]+ NoPerspective.*OpDecorate %[0-9]+ NoPerspective
// DISASM-MATCH: OpDecorate %[0-9]+ Centroid.*OpDecorate %[0-9]+ Centroid
// DISASM-MATCH: OpDecorate %[0-9]+ Sample.*OpDecorate %[0-9]+ Sample
//
// One field per qualifier. Location 0 is unqualified and Location 2 is
// center_perspective, the default, which decorates nothing. Both stages share
// the struct, so the Output and the Input variable carry the decoration: each
// kind appears on at least two variables, one per stage.
struct V {
    float4 p [[position]];
    float z;
    float a [[flat]];
    float b [[center_perspective]];
    float c [[center_no_perspective]];
    float d [[centroid_perspective]];
    float e [[centroid_no_perspective]];
    float f [[sample_perspective]];
    float g [[sample_no_perspective]];
};

vertex V interpolation_vs(uint vid [[vertex_id]])
{
    V o;
    o.p = float4(0.0);
    o.z = 1.0;
    o.a = 1.0;
    o.b = 1.0;
    o.c = 1.0;
    o.d = 1.0;
    o.e = 1.0;
    o.f = 1.0;
    o.g = 1.0;
    return o;
}

fragment float4 interpolation_fs(V in [[stage_in]])
{
    return float4(in.a + in.b + in.c, in.d + in.e, in.f + in.g, in.z);
}
