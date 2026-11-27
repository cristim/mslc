// EXPECT: valid
// DISASM: BuiltIn FragDepth
// DISASM: OpSelectionMerge
// DISASM: OpStore %gl_FragDepth
struct O { float d [[depth(any)]]; };
struct I { float4 p [[position]]; };
fragment O f(I i [[stage_in]]) { O o; if (i.p.x < 1.0) { o.d = 0.25; return o; } o.d = 0.375; return o; }
