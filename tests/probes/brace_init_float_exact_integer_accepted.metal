// EXPECT: valid
// DISASM: = OpConstantComposite %v2float %float_16777216 %float_1

//
// 16777216 is exactly a float, so it is not a narrowing.
constant float2 kValue = { 16777216, 1 };

kernel void brace_init_float_exact_integer_accepted(device float* out [[buffer(0)]],
                                                    uint i [[thread_position_in_grid]])
{
    out[i] = kValue.x;
}
