// EXPECT: valid
// DISASM-MATCH: OpEntryPoint Vertex %[_0-9a-zA-Z]+ "vertex_project"
// DISASM-MATCH: OpEntryPoint Fragment %[_0-9a-zA-Z]+ "fragment_flatcolor"
// DISASM-MATCH: OpDecorate %[_0-9a-zA-Z]+ DescriptorSet 0
// DISASM-MATCH: OpDecorate %[_0-9a-zA-Z]+ DescriptorSet 1
// DISASM-MATCH: OpExecutionMode %[_0-9a-zA-Z]+ OriginUpperLeft
// DISASM-NO-MATCH: OriginLowerLeft
// REFLECT: "name": "vertex_project"
// REFLECT: "name": "fragment_flatcolor"
// REFLECT: "stage": "vertex"
// REFLECT: "stage": "fragment"
//
// One source, two entry points, one module. Metal's newLibraryWithSource: takes
// a whole file, so a .metal carrying a vertex and a fragment function is one
// library and not a choice between them; mslc used to reject that outright
// ("source declares more than one entry point"), which is what stopped
// test/cube and test/triangle in indium's corpus.
//
// Four things have to be right, and the disassembly pins all of them:
//
//   - two OpEntryPoint instructions, each naming its own function, so the
//     module has two pipelines to create rather than one;
//   - two address blocks, one per set, because indium builds set 0 from the
//     vertex function and set 1 from the fragment function. A single cached
//     block would hand the fragment function the vertex function's set, and
//     its buffers would never bind, which is why the block is keyed by set
//     rather than cached once. Both sets have to appear, and that they do is
//     the whole claim: a module that declared only set 1 would pass a probe
//     that pinned the fragment half alone.
//   - the execution mode on the fragment entry point and not the vertex one,
//     since OriginUpperLeft is only legal on a fragment entry point.
//   - two entries in the reflection, one per entry point, because the
//     reflection is what indium reads to bind a pipeline and reporting one
//     entry point's bindings for a two-entry-point module would send it to the
//     wrong set.
//
// Neither function takes or returns a stage interface, so what is exercised
// here is the module-level threading and not the [[stage_in]] half, which is a
// separate capability: a user struct as a stage output needs attributes to
// become interface decorations, and no vertex-to-fragment data path is claimed
// by this probe.
vertex void vertex_project(device const float* in [[buffer(0)]],
                            device float* out [[buffer(1)]],
                            uint vid [[vertex_id]])
{
    out[vid] = in[vid];
}

fragment void fragment_flatcolor(device const float* in [[buffer(0)]],
                                 device float* out [[buffer(1)]],
                                 bool front [[front_facing]])
{
    if (front) {
        out[0] = in[0];
    } else {
        out[0] = 0.0;
    }
}
