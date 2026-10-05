// EXPECT: error rather than a struct, which is not lowered yet
//
// Apple takes a bare float4 from a vertex function as its position. mslc lowers
// the struct form only, and says so rather than writing the value somewhere.
vertex float4 vertex_returning_a_vector_rejected(uint vid [[vertex_id]])
{
    return float4(1.0);
}
