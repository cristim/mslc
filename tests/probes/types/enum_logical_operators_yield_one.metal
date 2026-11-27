// EXPECT: valid
// REFLECT: "metal_index": 1
// REFLECT: "metal_index": 3
//
// && and || give 1 or 0 whatever their operands are: A is 1, not 5, and B is
// 1, not 3, so the buffers take indices 1 and 1 + 2.
enum { A = 5 || (1 / 0), B = 2 && 3 };
kernel void enum_logical_operators_yield_one(device uint *a [[buffer(A)]], device uint *b [[buffer(B + 2)]], uint i [[thread_position_in_grid]])
{
    a[i] = b[i];
}
