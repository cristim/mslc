// EXPECT: valid
// DISASM-ORDER: %float_22 Aligned 4
// DISASM-ORDER: %float_33 Aligned 4
// DISASM-NO-MATCH: %float_11 Aligned
struct S { float a; float b; }; struct T { float a; float b; float c; }; struct P { float a; float b; }; struct O { P p; float c; };
kernel void k(device float* out [[buffer(0)]]) { S s{.b=(out[1]=11.0f,1.0f),.a=(out[2]=22.0f,2.0f),.b=(out[3]=33.0f,3.0f)}; out[0]=s.a; out[4]=s.b; }
