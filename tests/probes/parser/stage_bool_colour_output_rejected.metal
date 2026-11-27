// EXPECT: error is a bool, which mslc does not pass
// Apple rejects a bool colour output, so it is not a stage crossing.
struct F { float4 c [[color(0)]]; bool b [[color(1)]]; };
fragment F stage_bool_colour_output_rejected() { F o; o.c = float4(0.0); o.b = true; return o; }
