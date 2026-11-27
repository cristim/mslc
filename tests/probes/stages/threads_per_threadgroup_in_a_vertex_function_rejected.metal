// EXPECT: error builtin "threads_per_threadgroup" is not available in a vertex function
vertex float4 threads_per_threadgroup_in_a_vertex_function_rejected(uint t [[threads_per_threadgroup]])
{
    return float4(float(t));
}
