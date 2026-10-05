// EXPECT: error the enum type "Mode" is not supported as a type
//
// Apple accepts "Mode m = B;" and rejects "Mode m = 1;". mslc has no enum type, and treating it as int would accept the second, so the type is refused and the enumerators stay usable.
#include <metal_stdlib>
using namespace metal;
enum Mode { A, B };
kernel void enum_type_as_local_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ Mode m = B; out[i] = m; }
