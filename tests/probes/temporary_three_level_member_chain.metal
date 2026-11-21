// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpCompositeExtract %_struct_[0-9]+ %[0-9]+ 0
// DISASM-CAPTURE: = OpCompositeExtract %_struct_[0-9]+ %[0-9]+ 0[^=]*(%[0-9]+) = OpCompositeExtract %_struct_[0-9]+ %[0-9]+ 0
// DISASM-MATCH-CAPTURED: $2 = OpCompositeExtract %_struct_[0-9]+ $1 0
// DISASM-MATCH-CAPTURED: = OpCompositeExtract %int $2 0
struct A{int v;}; struct B{A a;}; struct C{B b;};
kernel void k(device int* out [[buffer(0)]]) { out[0] = C{B{A{7}}}.b.a.v; }
