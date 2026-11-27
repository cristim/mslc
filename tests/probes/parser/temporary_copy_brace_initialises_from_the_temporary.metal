// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpCompositeConstruct %_struct_[0-9]+ %int_1 %int_2
// DISASM-MATCH-CAPTURED: OpStore %[0-9]+ $1
// DISASM-NOT: OpCompositeExtract
struct S{int a;int b;};
kernel void k(device int* out [[buffer(0)]]) { S s{S{1,2}}; out[0]=s.b; }
