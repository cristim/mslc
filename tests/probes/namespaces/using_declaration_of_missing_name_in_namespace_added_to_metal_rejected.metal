// EXPECT: error no member named "bar" in the namespace "metal::mine"
// Apple: no member named 'bar' in namespace 'metal::mine'
#include <metal_stdlib>
namespace metal { namespace mine { void foo() {} } }
using metal::mine::bar;
kernel void using_declaration_of_missing_name_in_namespace_added_to_metal_rejected(device int* out [[buffer(0)]])
{
    out[0] = 1;
}
