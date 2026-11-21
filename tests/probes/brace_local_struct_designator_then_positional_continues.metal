// EXPECT: valid
// DISASM-MATCH: OpCompositeConstruct %_struct_[0-9]+ %float_3 %float_1 %float_2
struct S { float a; float b; }; struct T { float a; float b; float c; }; struct P { float a; float b; }; struct O { P p; float c; };
kernel void k(device float* out [[buffer(0)]]) { T t{.b=1.0f,2.0f,.a=3.0f}; out[0]=t.a; out[1]=t.b; out[2]=t.c; }
