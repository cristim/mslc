// EXPECT: valid
// DISASM-MATCH: = OpFUnordNotEqual %bool %[_0-9a-zA-Z]+ %float_0[^_0-9a-zA-Z]
// DISASM-MATCH: = OpINotEqual %bool %[_0-9a-zA-Z]+ %int_0[^_0-9a-zA-Z]
// DISASM-MATCH: = OpPhi %bool %false %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+
// RUN: grid=64 local=16 cases=9
// Apple flushes a float32 denormal in a compare and in bool(x) (#377); the seeded inputs
// contain no float32 denormals, so both backends agree at nine cases (measured 2026-11).

//
// && compares each numeric operand with zero, the right one in the block that
// is entered only when the left is non-zero.
kernel void logical_and_of_numeric_operands(device uint *out [[buffer(0)]], constant float *a [[buffer(1)]], constant int *b [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    if (a[i] && b[i]) {
        out[i] = 1u;
    }
}
