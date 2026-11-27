// EXPECT: valid
// DISASM: OpConstantComposite %v4int %int_1 %int_2 %int_3 %int_4
// DISASM: OpConstantComposite %_arr_int_uint_3 %int_5 %int_6 %int_7
//
// A vector and an array keep their positional brace list without "=", as Apple accepts them (#390).
#include <metal_stdlib>
using namespace metal;

constant int4 v{1, 2, 3, 4};
constant int a[3]{5, 6, 7};

kernel void file_scope_constant_brace_vector_and_array_valid(device int4 *out [[buffer(0)]])
{
    out[0] = v + int4(a[0], a[1], a[2], 0);
}
