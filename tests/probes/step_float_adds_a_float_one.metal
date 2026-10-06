// EXPECT: valid
// DISASM-MATCH: = OpConvertSToF %float %int_1
// DISASM-MATCH: = OpFAdd %float %[0-9]+ %[0-9]+
//
// f++ for a float adds 1.0.
kernel void step_float_adds_a_float_one(device float *out [[buffer(0)]],
                                        uint i [[thread_position_in_grid]])
{
    float x = out[i];
    x++;
    out[i] = x;
}
