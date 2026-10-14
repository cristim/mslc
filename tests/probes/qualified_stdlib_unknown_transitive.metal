// EXPECT: error metal::ghost
#include <metal_stdlib>
namespace N { using namespace ::metal; }
namespace Outer { using namespace N; }
int ghost(int value) { return value + 70; }
kernel void k(device int* out [[buffer(0)]], const device int* input [[buffer(1)]]) {
    out[0] = Outer::ghost(input[0]);
}
