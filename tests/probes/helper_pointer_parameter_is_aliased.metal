// EXPECT: valid
// DISASM-MATCH: OpDecorate %[A-Za-z0-9_]+ Aliased
// DISASM-NOT: Restrict
// A device or constant pointer parameter is a PhysicalStorageBuffer pointer, which
// SPV_KHR_physical_storage_buffer wants decorated Aliased or Restrict. Metal pointers may
// alias (f(out, out) is legal), so it is Aliased and a driver may not reorder the accesses.
#include <metal_stdlib>
using namespace metal;
float both(device float* a, device float* b, uint i) { a[i] = 1.0f; return b[i]; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = both(out, out, i); }
