// EXPECT: valid
// DIFFCHECK: writes-zero
// DISASM-MATCH: OpCompositeConstruct %_struct_[0-9]+ %int_1[_0-9]* %int_2[_0-9]*
struct S{int a;int b;}; typedef S T;
kernel void k(device int* out [[buffer(0)]]) { T{1,2}; out[0] = 0; }
