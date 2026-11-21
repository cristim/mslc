// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpCompositeConstruct %_struct_[0-9]+ %int_3 %int_4
// DISASM-MATCH-CAPTURED: = OpCompositeExtract %int $1 1
struct S{int a;int b;};
kernel void k(device int* out [[buffer(0)]]) { out[0] = (S{1,2}, S{3,4}).b; }
