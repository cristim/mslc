// EXPECT: error a file-scope atomic constant is not lowered
//
// Apple rejects a file-scope constant atomic_int (#390).
#include <metal_stdlib>
using namespace metal;

constant atomic_int a{0};

kernel void file_scope_constant_brace_atomic_rejected(device int *out [[buffer(0)]])
{
    out[0] = 1;
}
