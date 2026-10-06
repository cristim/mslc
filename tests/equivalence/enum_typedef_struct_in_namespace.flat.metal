#include <metal_stdlib>
using namespace metal;
enum Slot { kA = 0, kB = 3 };
typedef enum { kMode0, kMode1 = 4 } Mode;
typedef float3 Vec;
struct Item { Vec v; Mode m; };
kernel void k(device float* out [[buffer(kA)]], device const Item* in [[buffer(kB)]],
              uint i [[thread_position_in_grid]])
{
  Item it = in[i];
  Mode m = kMode1;
  out[i] = it.v.x + float(m) + float(kB);
}
