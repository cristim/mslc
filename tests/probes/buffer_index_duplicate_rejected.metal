// EXPECT: error the buffer index 1 is used by more than one parameter
//
// Apple: "cannot reserve 'buffer' resource location at index 1".
#include <metal_stdlib>
using namespace metal;
kernel void buffer_index_duplicate_rejected(device uint* a [[buffer(1)]], device uint* b [[buffer(1)]], uint i [[thread_position_in_grid]])
{ a[i] = b[i]; }
