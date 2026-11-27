// EXPECT: error metal::Ghost
#include <metal_stdlib>
typedef int Ghost;
kernel void k(device int* out [[buffer(0)]], const device int* input [[buffer(1)]]) {
    out[0] = metal::Ghost(input[0]);
}
