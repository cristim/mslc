// EXPECT: error is a long, which mslc does not pass
// Apple: "invalid return type for vertex function": 64-bit integers do not cross.
struct VOut { float4 position [[position]]; long f; };
vertex VOut stage_long_field_rejected(uint i [[vertex_id]]) { VOut o; o.position = float4(0.0); o.f = long(1); return o; }
