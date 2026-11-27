// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpDot %float
// DISASM-MATCH-CAPTURED: OpDecorate $1 NoContraction
//
// air.dot carries no fast-math flags in Apple's safe math, so the driver must not
// re-associate the sum of products (#371). A vector*scalar is an OpFMul on a splat in
// mslc, which #368 already decorates.
kernel void safe_math_dot_decorated(device float *out [[buffer(0)]],
    device const float4 *a [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = dot(a[i], a[i + 1]);
}
