// EXPECT: valid
// DISASM-MATCH: = OpFUnordNotEqual %bool %[_0-9a-zA-Z]+ %float_0[^_0-9a-zA-Z]
// DISASM-NOT: OpFOrdNotEqual
// RUN: grid=64 local=16 cases=9
// Apple flushes a float32 denormal in a compare and in bool(x) (#377); the seeded inputs
// contain no float32 denormals, so both backends agree at nine cases (measured 2026-11).

//
// bool(f) is f != 0 and a NaN converts to true, as it does in C++.
kernel void bool_from_float_is_unordered_compare(device uint *out [[buffer(0)]], constant float *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool b = bool(v[i]);
    uint r = 0u;
    if (b) { r = 1u; }
    out[i] = r;
}
