// EXPECT: valid
// DISASM: OpFunctionCall
#include <metal_stdlib>
using namespace metal;
struct Inner { float v; float arr[2]; float twice() const { return v * 2.0f; } };
struct Outer { float base; Inner operator[](uint i) const { return Inner{ base + float(i), { 3.0f, 7.0f } }; } };
array<Outer, 2> makeArr() { return array<Outer, 2>{{ { 2.0f }, { 6.0f } }}; }
kernel void struct_operator_subscript_member_and_array(device float* out [[buffer(0)]], constant bool& cond [[buffer(1)]]) {
    Outer s = { 5.0f }; array<Outer, 2> pair = {{ { 1.0f }, { 4.0f } }}; const thread float& r = s[0].v;
    out[0] = s[0].v + s[0].arr[1] + s[0].twice() + float(sizeof(s[0].v)) + r
        + (array<Outer, 2>{{ { 9.0f }, { 8.0f } }})[0][0].v + (cond ? pair : pair)[0][1].v + (0, pair)[1][0].v + makeArr()[0][1].v;
}
