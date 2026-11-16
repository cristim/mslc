// EXPECT: valid
// A buffer of empty structs: Metal's sizeof is 1, so the runtime array's stride is 1 and not
// the 0 that spirv-val rejects.
// DISASM-MATCH: OpDecorate %_runtimearr__struct_[0-9]+ ArrayStride 1
struct Emp {};
kernel void k(device Emp* in [[buffer(0)]], device float* out [[buffer(1)]])
{
    Emp e = in[0];
    in[1] = e;
    out[0] = 1.0f;
}
