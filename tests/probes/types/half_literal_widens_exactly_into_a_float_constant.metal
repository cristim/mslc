// EXPECT: valid
// DISASM-MATCH: = OpConstant %float 0\.0999755859
// DISASM-NOT: %float_0_1
//
// A float constant initialised with 0.1h holds the half nearest 0.1 widened,
// 0.0999756, not the float 0.1.
constant float k = 0.1h;

kernel void half_literal_widens_exactly_into_a_float_constant(device float *out [[buffer(0)]],
                                                              uint i [[thread_position_in_grid]])
{
    out[i] = k;
}
