// EXPECT: error not valid on a vertex attribute
// Apple accepts this and ignores the qualifier; mslc rejects it rather than drop it silently.
struct I { float4 p [[attribute(0), flat]]; };
vertex float4 interpolation_on_vertex_attribute_rejected(I in [[stage_in]]) { return in.p; }
