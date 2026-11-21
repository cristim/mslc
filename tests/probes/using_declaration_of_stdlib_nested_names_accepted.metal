// EXPECT: valid
// Real names in the library's nested namespaces and enums, a reserved-looking one,
// and a file that reopens namespace metal: Apple accepts every one of these.
#include <metal_stdlib>
namespace metal { namespace mine { void foo() {} } }
using metal::precise::sqrt;
using metal::fast::sqrt;
using metal::access::write;
using metal::mem_flags::mem_threadgroup;
using metal::mine::foo;
using metal::_texture2d;
kernel void using_declaration_of_stdlib_nested_names_accepted(device float* out [[buffer(0)]])
{
    out[0] = 1.0f;
}
