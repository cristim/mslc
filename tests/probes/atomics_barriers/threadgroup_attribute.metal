// EXPECT: valid
// DISASM-CAPTURE: OpDecorate (%[A-Za-z0-9_]+) SpecId 3
// DISASM-MATCH-CAPTURED: $1 = OpSpecConstant %uint 1
// DISASM-MATCH-CAPTURED: OpTypeArray %float $1
// DISASM-MATCH: OpVariable %_ptr_Workgroup__arr_float_[A-Za-z0-9_]+ Workgroup
// DISASM-NOT: PhysicalStorageBuffer %_arr_float
// REFLECT: { "kind": "ThreadgroupMemory", "metal_index": 0, "spec_id": 3, "element_size": 4, "param_index": 0, "name": "scratch" }
//
// A threadgroup memory argument. Metal sets its byte length at encode time, so the
// Workgroup array's length is a specialization constant the host sets to the element
// count (bytes / element_size) when it creates the pipeline. SpecIds 0 to 2 are the
// workgroup size, so the first argument is 3.
kernel void threadgroup_attribute(threadgroup float* scratch [[threadgroup(0)]],
                                  uint i [[thread_position_in_threadgroup]])
{ scratch[i] = 0.0; }
