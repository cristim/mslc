// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpCompositeConstruct %_struct_[0-9]+ %float_1 %float_2
// DISASM-CAPTURE: (%[0-9]+) = OpCompositeConstruct %_struct_[0-9]+ %[0-9]+ %float_3
// DISASM-MATCH-CAPTURED: OpCompositeConstruct %_struct_[0-9]+ $2 %float_4
struct C { float x; float y; }; struct B { C c; float z; }; struct A { B b; float w; };
kernel void k(device float* out [[buffer(0)]]) { A o{1.0f,2.0f,3.0f,4.0f}; out[0]=o.w; }
