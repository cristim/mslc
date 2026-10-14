// EXPECT: valid
// DISASM-MATCH: OpSelect %int %true[_0-9]* %int_7[_0-9]* %int_1[_0-9]*
// DISASM-NOT: OpFunctionCall
#include <metal_stdlib>
namespace N {using metal::select;}
int select(int a,int b,bool c){return 99;}
kernel void k(device int *o [[buffer(0)]]){o[0]=N::select(1,7,true);}
