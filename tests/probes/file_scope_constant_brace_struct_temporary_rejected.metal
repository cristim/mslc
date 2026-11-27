// EXPECT: error a constant of "int" cannot be initialised from "S"
//
// A struct temporary S{5} is not a scalar brace list (#390).
struct S { int x; };
constant int k{S{5}};

kernel void file_scope_constant_brace_struct_temporary_rejected(device int *out [[buffer(0)]])
{
    out[0] = k;
}
