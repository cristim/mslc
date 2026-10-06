// EXPECT: error reuses [[color(0)]]
// Apple: invalid return type.
struct O { float4 a [[color(0)]]; float4 b [[color(0)]]; };
fragment O color_index_duplicate_rejected() { O o; o.a = float4(0.0); o.b = float4(1.0); return o; }
