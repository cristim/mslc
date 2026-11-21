// EXPECT: valid
// DISASM-ORDER: %float_44 Aligned 4
// DISASM-ORDER: %float_55 Aligned 4
// DISASM-ORDER: %float_55 Aligned 4
// DISASM-ORDER: %float_33 Aligned 4
// DISASM-NO-MATCH: %float_11 Aligned
// DISASM-NO-MATCH: %float_22 Aligned
struct S { float a; float b; }; struct T { float a; float b; float c; }; struct P { float a; float b; }; struct O { P p; float c; };
kernel void k(device float* out [[buffer(0)]]) { O o{(out[1]=11.0f,1.0f),(out[2]=22.0f,2.0f),(out[3]=33.0f,3.0f),.p={(out[4]=44.0f,4.0f),(out[5]=55.0f,5.0f)}}; out[0]=o.p.a; out[6]=o.p.b; out[7]=o.c; }
