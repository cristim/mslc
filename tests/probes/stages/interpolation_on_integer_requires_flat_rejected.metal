// EXPECT: error requires [[flat]]
// Apple: type 'uint' requires 'flat' interpolation qualifier.
struct V { float4 p [[position]]; uint a [[center_perspective]]; };
vertex V interpolation_on_integer_requires_flat_rejected() { V o; o.p = float4(0.0); o.a = 1u; return o; }
