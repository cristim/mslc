// EXPECT: valid
// DISASM-MATCH: OpCompositeConstruct %_struct_[0-9]+ %int_7[_0-9]* %int_8[_0-9]*.*OpCompositeConstruct %_struct_[0-9]+ %int_9[_0-9]* %int_1[_0-9]*.*OpPhi %_struct_[0-9]+
// DISASM-NO-MATCH: OpCompositeConstruct %_struct_[0-9]+ %int_3[_0-9]* %int_4[_0-9]*.*OpCompositeConstruct %_struct_[0-9]+ %int_3[_0-9]* %int_4[_0-9]*
struct S{int a;int b;}; typedef S T; typedef T U;
kernel void k(device int* out [[buffer(0)]], constant int& nn [[buffer(1)]]) { bool c = nn>0; U u{T{3,4}}; out[0] = (c ? T{7,8} : U{9,1}).b + u.a; }
