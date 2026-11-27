// EXPECT: valid
// DISASM-MATCH: OpTypeArray %float %uint_1
// DISASM-MATCH: OpTypeArray %float %uint_200
//
// Apple takes a bool constant (true is 1) and any integer type as a length.
constant bool kOne = true;
constant uchar kBig = 200;

kernel void array_length_named_constant_bool_and_char(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    float a[kOne];
    float b[kBig];
    a[0] = 1.0; b[199] = 2.0;
    out[i] = a[0] + b[199];
}
