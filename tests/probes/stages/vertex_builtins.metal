// EXPECT: valid
// DISASM: BuiltIn VertexIndex
// DISASM: BuiltIn InstanceIndex
vertex void vertex_builtins(device uint* out [[buffer(0)]],
                            uint vid [[vertex_id]],
                            uint iid [[instance_id]])
{ out[vid] = iid; }
