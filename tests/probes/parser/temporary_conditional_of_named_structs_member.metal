// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpSelect %_struct_[0-9]+ %[0-9]+ %[0-9]+ %[0-9]+
// DISASM-MATCH-CAPTURED: = OpCompositeExtract %int $1 1
struct S{int a;int b;};
kernel void k(device int* out [[buffer(0)]], constant int& nn [[buffer(1)]]) { bool c = nn > 0; S p{1,2}; S q{3,4}; out[0] = (c ? p : q).b; }
