// EXPECT: error [[point_size]] on a fragment input
//
// A struct carrying [[point_size]] used as a vertex output is also taken as a
// fragment [[stage_in]] parameter; mslc does not lower point_size as a
// fragment input (per the plan's conservative default).
struct Out { float4 p [[position]]; float s [[point_size]]; };
vertex Out vs(uint vid [[vertex_id]]) { Out o; o.p = float4(0.0); o.s = 1.0; return o; }
fragment float4 fs(Out in [[stage_in]]) { return float4(in.s); }
