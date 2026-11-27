// EXPECT: valid
// DISASM: OpStore %gl_FragDepth
struct O { float d [[depth(any)]]; };
O make_depth() { O o; o.d = 0.25; return o; }
fragment O f() { return make_depth(); }
