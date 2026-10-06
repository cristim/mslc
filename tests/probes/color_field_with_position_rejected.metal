// EXPECT: error has [[position]] or [[attribute(n)]], which is not valid on a fragment output
// Apple: a field cannot be both [[color(n)]] and [[position]].
struct O { float4 a [[color(0), position]]; };
fragment O color_field_with_position_rejected() { O o; o.a = float4(0.0); return o; }
