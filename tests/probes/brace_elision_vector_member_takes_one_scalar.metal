// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpCompositeConstruct %v2float %float_2 %float_2
// DISASM-MATCH-CAPTURED: OpCompositeConstruct %_struct_[0-9]+ %float_1 $1 %float_3
struct V { float x; float2 v; float y; };
kernel void k(device float* out [[buffer(0)]]) { V o{1.0f,2.0f,3.0f}; out[0]=o.y; }
