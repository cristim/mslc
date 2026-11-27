// EXPECT: error discard_fragment() is not allowed within a vertex function
// Apple: "vertex function 'vs' uses unsupported builtin functions".
#include <metal_stdlib>
using namespace metal;
float maybe_drop(float x) { if (x > 0.5f) discard_fragment(); return x; }
vertex float4 discard_helper_called_from_vertex_rejected(uint v [[vertex_id]]) { return float4(maybe_drop(0.75f)); }
