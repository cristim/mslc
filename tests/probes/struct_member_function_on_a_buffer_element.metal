// EXPECT: valid
// DISASM: OpFunctionCall
struct S { float2 a; float4 b; float sum() const { return a.x + b.w; } };
kernel void k(device float *out [[buffer(0)]], device const S *in [[buffer(1)]])
{
    S s = in[0];
    out[0] = s.sum();
}
