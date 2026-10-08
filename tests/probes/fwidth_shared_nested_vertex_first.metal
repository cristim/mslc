// EXPECT: error fwidth is only available in fragment functions
struct In { float4 position [[position]]; };
struct Out { float4 position [[position]]; };
float inner(float x) { return fwidth(x); }
float outer(float x) { return inner(x); }
vertex Out vert() { Out o; o.position = float4(outer(1.0f)); return o; }
fragment float4 frag(In p [[stage_in]]) { return float4(outer(p.position.x)); }
