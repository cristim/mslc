// EXPECT: valid
// DISASM-MATCH: OpTypeInt 32 0
//
// const unsigned int, as MDP3DLine spells vertex_id.
kernel void k(device uint* o [[buffer(0)]], const unsigned int vid [[thread_position_in_grid]])
{
    o[vid] = vid;
}
