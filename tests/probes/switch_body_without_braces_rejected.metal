// EXPECT: error a switch body needs braces around its case labels

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  switch (int(gid)) out[gid] = 1;
}
