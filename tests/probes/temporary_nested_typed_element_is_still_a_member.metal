// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpCompositeConstruct %_struct_[0-9]+ %int_1 %int_2
// DISASM-MATCH-CAPTURED: = OpCompositeConstruct %_struct_[0-9]+ $1 %int_3
// DISASM-NO-MATCH: OpCompositeConstruct %_struct_[0-9]+ %int_1 %int_2.*OpCompositeConstruct %_struct_[0-9]+ %int_1 %int_2
struct I{int x;int y;}; struct O{I i;int z;};
kernel void k(device int* out [[buffer(0)]]) { O o{I{1,2},3}; out[0]=o.i.y; }
