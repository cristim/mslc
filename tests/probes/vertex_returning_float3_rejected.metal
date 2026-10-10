// EXPECT: error a float4 position
// Only a float4 is the vertex position; any other bare vector is still refused.
vertex float3 vertex_returning_float3_rejected() {
    return float3(1.0);
}
