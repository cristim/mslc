// EXPECT: valid
// DISASM-MATCH: OpDecorate %[0-9]+ Location 7
// Apple: the valid range of [[color(n)]] is 0 to 7, so 7 is accepted.
struct O { float4 a [[color(0)]]; float4 b [[color(7)]]; };
fragment O fragment_color_index_seven_is_the_top_valid() { O o; o.a = float4(0.0); o.b = float4(1.0); return o; }
