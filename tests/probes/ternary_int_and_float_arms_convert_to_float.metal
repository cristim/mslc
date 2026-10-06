// EXPECT: valid
// DISASM: = OpSelect %float
// DISASM: = OpConvertSToF %float
// DISASM-NOT: = OpSelect %int
//
// An int arm beside a float arm is converted to float, as in C: the result is a
// float whichever arm is chosen.
kernel void ternary_int_and_float_arms_convert_to_float(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    int n = int(i);
    float f = in[0];
    out[i] = in[1] > 0.0f ? n : f;
}
