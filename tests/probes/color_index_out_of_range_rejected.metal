// EXPECT: error allows indices 0 to 7
// Apple: 'color' attribute parameter is out of bounds: must be between 0 and 7.
struct O { float4 a [[color(8)]]; };
fragment O color_index_out_of_range_rejected() { O o; o.a = float4(0.0); return o; }
