// EXPECT: valid
// A block's using declaration of a stdlib name must still hide a closer declaration
// of the same name, exactly as a using declaration of a user name does: a struct's
// own member function here. Apple calls the builtin abs(int), not the member.
// DISASM-MATCH: OpExtInst %int %[0-9]+ SAbs %
// DISASM-NO-MATCH: OpReturnValue %int_99
#include <metal_stdlib>
using namespace metal;
struct T {
    int abs(int) const { return 99; }
    int call() const { using metal::abs; return abs(-5); }
};
kernel void using_declaration_of_stdlib_name_hides_member_function(device int* out [[buffer(0)]])
{
    T t;
    out[0] = t.call();
}
