// EXPECT: valid
// DISASM: = OpConvertSToF %float
//
// A typedef is a cast target, as in cast_enum_and_typedef_names.
typedef float real;
kernel void static_cast_typedef_target(device float* out [[buffer(0)]], constant int* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = static_cast<real>(in[i]);
}
