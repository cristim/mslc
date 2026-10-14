// EXPECT: error metal::ghost_value
#include <metal_stdlib>
constant int ghost_value = 11;
kernel void k(device int* out [[buffer(0)]], const device int* input [[buffer(1)]]) {
    out[0] = metal::ghost_value;
}
