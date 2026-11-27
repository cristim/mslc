// EXPECT: valid
// DISASM: OpConstant %ulong 16
//
// A field of a struct that does not enclose the function does not hide S: here sizeof(S) in
// Other::size is the struct's 16 although Holder has a member named S. Apple's compiler agrees.
#include <metal_stdlib>
using namespace metal;
struct S { float4 a; };
struct Holder { char S; };
struct Other {
    int pad;
    ulong size() const { return sizeof(S); }
};
kernel void sizeof_struct_tag_not_hidden_by_other_struct_field(device ulong *out [[buffer(0)]])
{
    Holder h;
    h.S = 1;
    Other o;
    o.pad = 0;
    out[0] = o.size();
}
