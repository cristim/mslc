// EXPECT: error field "c" of "O" is a short, which mslc does not pass between stages yet
struct O { float4 p [[position]]; short c; };
vertex O stage_interface_short_vertex_output_rejected() {
    O o;
    o.p = float4(0);
    o.c = short(7);
    return o;
}
