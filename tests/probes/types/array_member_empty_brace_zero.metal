// EXPECT: valid
// DIFFCHECK: writes-zero
// DISASM-MATCH: OpConstantNull %_struct_[0-9]+
//
// "S s = {}" and "S t{}" zero a struct with an array member; the member-by-member
// path would build a struct from a scalar zero.
struct S { float a[2]; };

kernel void array_member_empty_brace_zero(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    S s = {};
    S t{};
    out[i] = s.a[1] + t.a[0];
}
