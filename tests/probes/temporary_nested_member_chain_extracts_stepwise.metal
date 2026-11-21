// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpCompositeConstruct %_struct_[0-9]+ %int_1 %int_2
// DISASM-CAPTURE: (%[0-9]+) = OpCompositeConstruct %_struct_[0-9]+ %[0-9]+ %int_3
// DISASM-MATCH-CAPTURED: $2 = OpCompositeConstruct %_struct_[0-9]+ $1 %int_3
// DISASM-CAPTURE: (%[0-9]+) = OpCompositeExtract %_struct_[0-9]+ %[0-9]+ 0
// DISASM-MATCH-CAPTURED: $3 = OpCompositeExtract %_struct_[0-9]+ $2 0
// DISASM-MATCH-CAPTURED: = OpCompositeExtract %int $3 1
struct I{int x;int y;}; struct O{I i;int z;};
kernel void k(device int* out [[buffer(0)]]) { out[0] = O{I{1,2},3}.i.y; }
