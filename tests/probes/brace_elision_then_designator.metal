// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpCompositeConstruct %_struct_[0-9]+ %float_1 %float_0
// DISASM-MATCH-CAPTURED: OpCompositeConstruct %_struct_[0-9]+ $1 %float_3
struct Pair { float a; float b; }; struct Outer { Pair p; float c; };
kernel void k(device float* out [[buffer(0)]]) { Outer o{1.0f,.c=3.0f}; out[0]=o.c; }
