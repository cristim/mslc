// Frozen string literal from Blender 4.5.14,
// source/blender/gpu/metal/mtl_context.mm:2583-2593. Runs on every GPU buffer
// clear.
struct BufferClearParams { uint clear_value; };
kernel void compute_buffer_clear(constant BufferClearParams &params [[buffer(0)]],
                                 device uint32_t* output_data [[buffer(1)]],
                                 uint position [[thread_position_in_grid]])
{ output_data[position] = params.clear_value; }
