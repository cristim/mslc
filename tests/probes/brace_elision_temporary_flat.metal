// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpCompositeConstruct %_struct_[0-9]+ %float_1 %float_2
// DISASM-MATCH-CAPTURED: OpCompositeConstruct %_struct_[0-9]+ $1 %float_3
struct Pair { float a; float b; }; struct Outer { Pair p; float c; };
float take(Outer o) { return o.c; }
kernel void k(device float* out [[buffer(0)]]) { out[0]=take(Outer{1.0f,2.0f,3.0f}); }
