// EXPECT: error not valid on a fragment output
// Apple accepts this and ignores the qualifier; mslc rejects it rather than drop it silently.
struct O { float4 c [[color(0), flat]]; };
fragment O interpolation_on_fragment_output_rejected() { O o; o.c = float4(0.0); return o; }
