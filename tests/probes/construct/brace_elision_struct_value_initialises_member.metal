// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpLoad %_struct_[0-9]+ %[0-9]+
// DISASM-MATCH-CAPTURED: = OpCompositeConstruct %_struct_[0-9]+ $1 %float_3
struct Pair { float a; float b; }; struct Outer { Pair p; float c; };
kernel void k(device float* out [[buffer(0)]]) { Pair q{1.0f,2.0f}; Outer o{q,3.0f}; out[0]=o.c; }
