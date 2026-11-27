// EXPECT: valid
// DISASM-CAPTURE: %int_0[^%]+(%[0-9]+) = OpLoad %float %[0-9]+ Aligned 4
// DISASM-CAPTURE: %int_1[^%]+(%[0-9]+) = OpLoad %float %[0-9]+ Aligned 4
// DISASM-CAPTURE: %int_2[^%]+(%[0-9]+) = OpLoad %float %[0-9]+ Aligned 4
// DISASM-CAPTURE: (%[0-9]+) = OpCompositeConstruct %_struct_[0-9]+ %[0-9]+ %[0-9]+
// DISASM-MATCH-CAPTURED: = OpCompositeConstruct %_struct_[0-9]+ $1 $2
// DISASM-MATCH-CAPTURED: = OpCompositeConstruct %_struct_[0-9]+ $4 $3
struct Pair { float a; float b; }; struct Outer { Pair p; float c; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { Outer o{in[0],in[1],in[2]}; out[0]=o.p.a+o.p.b*10+o.c*100; }
