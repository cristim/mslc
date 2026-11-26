// EXPECT: valid
// DISASM-MATCH: = OpFUnordNotEqual %bool %[_0-9a-zA-Z]+ %float_0[^_0-9a-zA-Z]
// DISASM-MATCH: OpBranchConditional %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+
// RUN: grid=64 local=16 cases=9
// Apple flushes a float32 denormal in a compare and in bool(x) (#377); the seeded inputs
// contain no float32 denormals, so both backends agree at nine cases (measured 2026-11).

//
// A numeric scalar is a condition by comparison with zero. It used to reach
// OpBranchConditional as a float, which spirv-val rejects.
kernel void condition_from_float_is_unordered_compare(device uint *out [[buffer(0)]], constant float *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    if (v[i]) {
        out[i] = 1u;
    }
}
