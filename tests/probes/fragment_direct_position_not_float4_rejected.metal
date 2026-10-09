// EXPECT: error has to be a float4
//
// A direct [[position]] parameter declared narrower than float4. Without the
// float4 type check on a FragCoord-mapped parameter, this silently aliases a
// float4 and a later use of p produces a confusing, unrelated error instead
// of a clear one at the parameter itself.
fragment float4 fragment_direct_position_not_float4_rejected(float2 p [[position]])
{
    return float4(p, 0.0, 1.0);
}
