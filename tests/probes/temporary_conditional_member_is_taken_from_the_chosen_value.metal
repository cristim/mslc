// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpPhi %_struct_[0-9]+ %[0-9]+ %[0-9]+ %[0-9]+ %[0-9]+
// DISASM-MATCH-CAPTURED: = OpCompositeExtract %int $1 1
struct S{int a;int b;};
kernel void k(device int* out [[buffer(0)]], constant int& nn [[buffer(1)]]) { bool c = nn > 0; out[0] = (c ? S{1,2} : S{3,4}).b; }
