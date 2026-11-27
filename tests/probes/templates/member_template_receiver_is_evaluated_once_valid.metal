// EXPECT: valid
// DISASM-NO-MATCH: OpFunctionCall %_struct_[0-9]+ %[_0-9a-zA-Z]+ %float_7_5.*OpFunctionCall %_struct_[0-9]+ %[_0-9a-zA-Z]+ %float_7_5
// Refs #341: a receiver that is a call is evaluated once, to learn its struct and to be the object, not again to rank the templates.
#include <metal_stdlib>
using namespace metal;
struct S { float a; template <typename U> float f(U x) const { return a + float(x); } };
S mk(float x) { S s{x}; return s; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = mk(7.5f).f(3); }
