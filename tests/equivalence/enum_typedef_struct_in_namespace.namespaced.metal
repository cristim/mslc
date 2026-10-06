#include <metal_stdlib>
using namespace metal;
namespace N {
  enum Slot { kA = 0, kB = 3 };
  typedef enum { kMode0, kMode1 = 4 } Mode;
  typedef float3 Vec;
  struct Item { Vec v; Mode m; };
}
kernel void k(device float* out [[buffer(N::kA)]], device const N::Item* in [[buffer(N::kB)]],
              uint i [[thread_position_in_grid]])
{
  N::Item it = in[i];
  N::Mode m = N::kMode1;
  out[i] = it.v.x + float(m) + float(N::kB);
}
