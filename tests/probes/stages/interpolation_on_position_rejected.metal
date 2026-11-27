// EXPECT: error [[position]], which cannot also have an interpolation attribute
struct V { float4 p [[position, flat]]; float a; };
vertex V interpolation_on_position_rejected() { V o; o.p = float4(0.0); o.a = 1.0; return o; }
