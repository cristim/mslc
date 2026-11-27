// EXPECT: error has no [[color(n)]]
// Apple: invalid return type.
struct O { float4 a [[color(0)]]; float4 b; };
fragment O color_field_missing_rejected() { O o; o.a = float4(0.0); o.b = float4(1.0); return o; }
