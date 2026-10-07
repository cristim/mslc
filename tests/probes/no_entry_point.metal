// EXPECT: error declares no function, and mslc emits one SPIR-V module per library
//
// The two Xcode Metal templates are this source with nothing but a
// metal_stdlib include, and Apple's compiler accepts both: it builds a library
// with no functions in it. mslc cannot, and the reason is worth naming. mslc
// emits one SPIR-V module per library and a Vulkan module needs an entry point:
// spirv-val rejects a module with no OpEntryPoint unless it declares the
// Linkage capability, which Vulkan does not allow. So there is no empty module
// to emit, and the message says that rather than leaving a reader to think the
// source is at fault.
struct Unused { uint x; };