// EXPECT: valid
// DISASM: OpConstant %int 3
// DISASM: OpConstant %int 4
// DISASM: OpConstant %int 7
// DISASM: OpConstant %int 9
//
// typedef enum Tag {...} Name, typedef enum {...} Name and the repeated tag all declare enumerators.
#include <metal_stdlib>
using namespace metal;
typedef enum Tagged { T0, T1 } Tagged;
typedef enum { N0 = 3, N1 } Untagged;
typedef enum Other { O0 = 7 } OtherName;
enum Plain { P0 = 9 };
kernel void enum_typedef_forms(device uint* out [[buffer(T1)]], uint i [[thread_position_in_grid]])
{ out[0] = T1 + N0 + N1 + O0 + P0; }
