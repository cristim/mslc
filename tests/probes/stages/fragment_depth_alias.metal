// EXPECT: valid
// DISASM: BuiltIn FragDepth
typedef float Depth;
struct O { Depth d [[depth(any)]]; };
fragment O f() { O o; o.d = 0.25; return o; }
