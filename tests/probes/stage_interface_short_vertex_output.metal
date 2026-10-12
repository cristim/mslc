// EXPECT: valid
// A short crosses as a signed 32-bit integer and is narrowed again, so it needs no 16-bit
// input/output storage; Apple accepts it.
struct O { float4 p [[position]]; short c; };
vertex O stage_interface_short_vertex_output() {
    O o;
    o.p = float4(0);
    o.c = short(7);
    return o;
}
