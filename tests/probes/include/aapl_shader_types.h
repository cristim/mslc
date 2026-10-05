#ifndef AAPL_SHADER_TYPES_H
#define AAPL_SHADER_TYPES_H

#include <simd/simd.h>

typedef enum AAPLVertexInputIndex
{
    AAPLVertexInputIndexVertices     = 0,
    AAPLVertexInputIndexViewportSize = 1,
} AAPLVertexInputIndex;

typedef struct
{
    vector_float2 position;
    vector_float4 color;
} AAPLVertex;

#endif
