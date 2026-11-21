// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpCompositeConstruct %_struct_[0-9]+ %int_1 %int_2
// DISASM-MATCH-CAPTURED: OpCompositeConstruct %_struct_[0-9]+ $1 %float_3
struct PI { int a; int b; }; struct OI { PI p; float c; };
kernel void k(device float* out [[buffer(0)]]) { OI o{1,2,3.0f}; out[0]=o.c; }
