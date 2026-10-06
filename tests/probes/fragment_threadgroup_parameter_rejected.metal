// EXPECT: error threadgroup parameters are only supported on kernel functions
//
// Apple accepts this; the old output was a Workgroup variable in a fragment
// shader, which means nothing, so it stays rejected.
fragment float4 fragment_threadgroup_parameter_rejected(threadgroup float* p)
{ return float4(p[0]); }
