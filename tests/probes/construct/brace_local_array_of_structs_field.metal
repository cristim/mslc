// EXPECT: valid
// RUN: grid=4 local=4 cases=3
//
// A local struct whose member is an array of structs, value-initialised by empty braces and then
// written element by element. Once refused ("an array of structs field in value struct").
struct E { float x; };
struct S { E values[2]; };
kernel void brace_local_array_of_structs_field(device float* out [[buffer(0)]], const device float* in [[buffer(1)]],
    uint i [[thread_position_in_grid]])
{
    S s {};
    s.values[1].x = in[i];
    out[i] = s.values[0].x + s.values[1].x * 2;
}
