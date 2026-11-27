// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpFMul %v4float
// DISASM-MATCH-CAPTURED: OpDecorate $1 NoContraction
//
// mslc lowers vector*scalar to OpFMul on a splatted vector. Apple emits a plain fmul per
// component under -fno-fast-math, so the product is decorated NoContraction (#371).
kernel void safe_math_vector_times_scalar_fmul_decorated(device float4 *out [[buffer(0)]],
    device const float4 *a [[buffer(1)]], device const float *s [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    out[i] = a[i] * s[0];
}
