// EXPECT: valid
// A block's using declaration of a stdlib name must still hide the enclosing
// namespace's own function of the same name. Apple calls the builtin abs(int).
// DISASM-MATCH: OpExtInst %int %[0-9]+ SAbs %
// DISASM-NO-MATCH: OpReturnValue %int_99
#include <metal_stdlib>
using namespace metal;
namespace N {
    int abs(int) { return 99; }
    int g() { using metal::abs; return abs(-5); }
}
kernel void using_declaration_of_stdlib_name_hides_namespace_function(device int* out [[buffer(0)]])
{
    out[0] = N::g();
}
