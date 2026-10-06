// EXPECT: valid
// DISASM: OpSLessThan
// DISASM-NOT: OpULessThan
//
// INT_MAX fits an int, so the enum is still an int and "A - A - 1 < 0" is a signed comparison (Apple: true).
#include <metal_stdlib>
using namespace metal;
enum One { A = 0x7fffffff };
kernel void enum_int_max_enumerator_stays_int(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = (A - A - 1) < 0; }
