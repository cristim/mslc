// EXPECT: error which requires [[flat]]
// Apple rejects a bool with an interpolation qualifier other than flat.
struct VOut { float4 position [[position]]; bool f [[center_perspective]]; };
vertex VOut stage_bool_interpolated_rejected(uint i [[vertex_id]]) { VOut o; o.position = float4(0.0); o.f = true; return o; }
