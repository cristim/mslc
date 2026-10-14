// EXPECT: error metal::ghost
#include <metal_stdlib>
namespace N { using namespace ::metal; }
namespace Alias = N;
int ghost(int value) { return value + 70; }
kernel void k(device int* out [[buffer(0)]], const device int* input [[buffer(1)]]) {
    out[0] = Alias::ghost(input[0]);
}
