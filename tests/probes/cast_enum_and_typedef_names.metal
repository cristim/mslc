// EXPECT: valid
// DISASM: = OpConvertSToF %float
//
// A typedef and an enum name are cast targets, as they are functional-cast
// targets; an enum is its underlying int.
typedef float real;
enum Mode { kA, kB };
kernel void cast_enum_and_typedef_names(device float* out [[buffer(0)]], constant int* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    Mode m = (Mode)in[0];
    out[i] = (real)(int)m;
}
