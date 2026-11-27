// EXPECT: error the constant "bad" has the enum type Mode, and an integer does not convert to it
//
// Apple rejects an integer in an enum constant's brace list (#390).
enum Mode { MA, MB };
constant Mode bad{1};

kernel void file_scope_constant_brace_enum_integer_rejected(device int *out [[buffer(0)]])
{
    out[0] = int(bad);
}
