// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpCompositeConstruct %_struct_[0-9]+ %float_2 %float_3
// DISASM-MATCH-CAPTURED: OpCompositeConstruct %_struct_[0-9]+ %float_1 $1 %float_4
struct Pair { float a; float b; }; struct Outer2 { float x; Pair p; float c; };
kernel void k(device float* out [[buffer(0)]]) { Outer2 o{1.0f,2.0f,3.0f,4.0f}; out[0]=o.c; }
