// EXPECT: valid
// DEFINE: VALUE=91
// A #define of the same name replaces a predefined macro; it is not a conflicting redefinition
#define VALUE 17
// DISASM: OpConstant %int 17
// DISASM-NOT: 91
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = VALUE; }
