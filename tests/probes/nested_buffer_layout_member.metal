// EXPECT: valid
// RUN: grid=4 local=4 cases=3
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 4
//
// A constant buffer of a struct whose member is a struct: the member is laid out whole, at 4 after
// the float before it, and read through the pointer's element. Once refused ("has a field whose type
// is itself a struct").
struct Inner { float a; };
struct Outer { float first; Inner v; };
kernel void nested_buffer_layout_member(device float* out [[buffer(0)]], constant Outer* in [[buffer(1)]],
    uint i [[thread_position_in_grid]]) { out[i] = in[i].v.a + in[i].first; }
