// EXPECT: valid
// DISASM: OpReturnValue %int_11
// DISASM: OpReturnValue %int_12
// DISASM-NO-MATCH: OpReturnValue %int_77
// DISASM-NO-MATCH: OpReturnValue %int_99
// A block's using declaration is searched before the struct's members, so x in get()
// is A::x (11) and f() is A::f (12, not the member's 99); a using directive's names are not, so x in field() is
// the member, not C::x (77). Apple accepts all three.
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; int f() { return 12; } }
namespace C { constant int x = 77; }
struct T {
    int x;
    int f() const { return 99; }
    int get() const { using A::x; return x; }
    int call() const { using A::f; return f(); }
    int field() const { using namespace C; return x; }
};
kernel void using_declaration_in_member_function_hides_field(device int* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    T t;
    t.x = int(i);
    out[0] = t.get() + t.call() + t.field();
}
