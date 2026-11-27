// EXPECT: valid
// DISASM-MATCH: OpConstant %ulong 2[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 4[^0-9]
//
// A float parameter y hides the file-scope half y in a helper, and ::y * ::y is the half (2),
// not the parameter's float (4). Apple: 2.
#include <metal_stdlib>
using namespace metal;
constant half y = 1.0h;
ulong size(float y) { return sizeof(::y * ::y); }
kernel void sizeof_expression_file_scope_name_parameter(device ulong *out [[buffer(0)]])
{
    out[0] = size(1.0f);
}
