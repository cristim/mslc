// EXPECT: error threadgroup parameters are only supported on kernel functions
//
// Apple: invalid type 'threadgroup float *' for buffer declaration in a vertex function.
vertex float4 threadgroup_argument_in_vertex_rejected(threadgroup float* a [[threadgroup(0)]]) { return float4(1.0); }
