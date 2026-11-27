// EXPECT: error the buffer index 1 is used by more than one parameter
//
// Two enumerators of one value are one slot.
#include <metal_stdlib>
using namespace metal;
enum { A = 1, B = 1 };
kernel void buffer_index_duplicate_through_enumerators_rejected(device uint* a [[buffer(A)]], device uint* b [[buffer(B)]], uint i [[thread_position_in_grid]])
{ a[i] = b[i]; }
