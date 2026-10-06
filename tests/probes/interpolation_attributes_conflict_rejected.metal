// EXPECT: error more than one interpolation attribute
struct V { float4 p [[position]]; float a [[flat, center_no_perspective]]; };
vertex V interpolation_attributes_conflict_rejected() { V o; o.p = float4(0.0); o.a = 1.0; return o; }
