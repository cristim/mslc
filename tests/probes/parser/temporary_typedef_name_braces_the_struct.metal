// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpCompositeConstruct %_struct_[0-9]+ %int_1[_0-9]* %int_2[_0-9]*
// DISASM-MATCH-CAPTURED: = OpCompositeExtract %int $1 1
struct S{int a;int b;}; typedef S T;
kernel void k(device int* out [[buffer(0)]]) { out[0] = T{1,2}.b; }
