// Metal side of diffcheck: compiles a kernel with Apple's compiler, writes Apple's own
// reflection of its buffers, and (in run mode) dispatches it and writes every buffer back.
//
// Manifest, one directive per line (same shape as vkhost's):
//   mode reflect|run          source <file.metal> | metallib <file>     entry <name>
//   std <2.4|3.0|3.1|3.2>     local X Y Z       groups X Y Z       reflection <out.json>
//   contract on|off           (source mode; default off, see below)
//   buffer <metal index> <bytes> <input file> <output file>          (run mode)
//   texture|storagetexture <metal index> <set> <binding> <w> <h> <rgba file> [<type> [write|readwrite <output file>]]
//                                                    (run mode; storage: mslc's storage image, written back when an
//                                                     output file is given; set and binding are vkhost's, ignored here)
//   image <kind> <access> <metal index> <set> <binding> <w> <h> <d> <layers> <type> <in> [<out>|mips=<n>]
//                                                    (run mode; non-2D or multi-mip texture: 1d, 1d_array, 2d, 2d_array, 3d, cube, cube_array)
//   sampler <metal index> <set> <binding>            (run mode; address mode = index % 3)
//   threadgroup <metal index> <bytes>   setThreadgroupMemoryLength:atIndex: (run mode)
//   texelbuffer <metal index> <set> <binding> uniform|storage <type> <components 1|2|4> <texels> <input file> <output file>
//                                       (run mode; a texture_buffer over a shared buffer of RGBA texels of <type>:
//                                        float half int uint short ushort; storage: shader read and write)
// A texture is RGBA of <type> (default float; shader read, or read and write when an output file is given); a
// sampler is nearest and normalised. A texel buffer's bytes are written back to its output file after the dispatch,
// like a device buffer's. A device that cannot make the texture exits 2 with "unsupported: <reason>", which the driver reports as a skip.
// Graphics: mode reflect-gfx|draw, see drawMain; reflect-gfx writes {vertex, fragment} binding
// lists (buffers with layouts, textures, samplers) of the render pipeline.
// The source is compiled with newLibraryWithSource and MTLMathModeSafe, the way Blender
// and Darling's callers do, plus `#pragma clang fp contract(off)`: Metal contracts
// a * b + c into an fma even in safe math mode, mslc emits a separate multiply and
// add, and the two differ by a ULP or more, which is a permitted difference and not a
// miscompile (`contract on` keeps the app's behaviour). A metallib (built by `xcrun metal`) is for sources that
// #include a local header, which an in-memory source cannot resolve.
//
// The reflection lists, per buffer: index, name, access, whether it is a pointer, the
// byte stride of one element, and every scalar leaf of the element type as {off, scalar[, field]}.
// Vectors and matrices are flattened to their components, structs and arrays are
// walked, so the driver needs no knowledge of Metal types. Arguments the harness cannot
// feed (textures, samplers, threadgroup memory, argument buffers) are listed in
// "unsupported". Exit 0 on success, 2 on any error (never a mismatch).
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <stdint.h>
#include <string>
#include <vector>

static void die(NSString *msg) { fprintf(stderr, "metalhost: %s\n", msg.UTF8String); exit(2); }

static uint32_t parseU32(NSString *s, NSString *what) {
    const char *p = s.UTF8String;
    if (!p || !*p) die([@"bad " stringByAppendingFormat:@"%@ in manifest", what]);
    uint64_t v = 0;
    for (; *p; p++) {
        if (*p < '0' || *p > '9') die([@"bad " stringByAppendingFormat:@"%@ in manifest: %@", what, s]);
        v = v * 10 + (uint64_t)(*p - '0');
        if (v > UINT32_MAX) die([@"bad " stringByAppendingFormat:@"%@ in manifest: %@", what, s]);
    }
    return (uint32_t)v;
}

struct Leaf { NSUInteger off; const char *scalar; };

// MTLDataType -> (scalar name, rows, columns). columns 1 is a vector, rows 1 a scalar.
static bool shape(MTLDataType t, const char **scalar, int *rows, int *cols, NSUInteger *colStride) {
#define V(S, N, T) case MTLDataType##T##N: *scalar = S; *rows = N; *cols = 1; *colStride = 0; return true;
#define SCALAR(S, T) case MTLDataType##T: *scalar = S; *rows = 1; *cols = 1; *colStride = 0; return true;
#define KIND(S, T) SCALAR(S, T) V(S, 2, T) V(S, 3, T) V(S, 4, T)
#define M(S, C, R, T) case MTLDataType##T##C##x##R: *scalar = S; *rows = R; *cols = C; *colStride = (R == 3 ? 4 : R) * (S[0] == 'h' ? 2 : 4); return true;
#define MATS(S, T) M(S, 2, 2, T) M(S, 2, 3, T) M(S, 2, 4, T) M(S, 3, 2, T) M(S, 3, 3, T) M(S, 3, 4, T) M(S, 4, 2, T) M(S, 4, 3, T) M(S, 4, 4, T)
    switch (t) {
        KIND("float", Float) KIND("half", Half) KIND("int", Int) KIND("uint", UInt) KIND("short", Short)
        KIND("ushort", UShort) KIND("char", Char) KIND("uchar", UChar) KIND("long", Long) KIND("ulong", ULong)
        KIND("bool", Bool)
        MATS("float", Float) MATS("half", Half)
        default: return false;
    }
}

static NSUInteger scalarSize(const char *s) {
    std::string n(s);
    if (n == "half" || n == "short" || n == "ushort") return 2;
    if (n == "char" || n == "uchar" || n == "bool") return 1;
    if (n == "long" || n == "ulong") return 8;
    return 4;
}

static NSMutableArray *g_unsupported;

// The element type and texture kind of a texture binding; anything else is "other".
static NSString *dataName(MTLDataType t) {
    switch (t) {
        case MTLDataTypeFloat: return @"float"; case MTLDataTypeHalf: return @"half";
        case MTLDataTypeInt: return @"int"; case MTLDataTypeUInt: return @"uint";
        case MTLDataTypeShort: return @"short"; case MTLDataTypeUShort: return @"ushort";
        default: return @"other";
    }
}

static NSString *textureTypeName(MTLTextureType t) {
    switch (t) {
        case MTLTextureType1D: return @"1d";
        case MTLTextureType1DArray: return @"1d_array";
        case MTLTextureType2D: return @"2d";
        case MTLTextureType2DArray: return @"2d_array";
        case MTLTextureType3D: return @"3d";
        case MTLTextureTypeCube: return @"cube";
        case MTLTextureTypeCubeArray: return @"cube_array";
        case MTLTextureTypeTextureBuffer: return @"texturebuffer";
        default: return @"other";
    }
}

static void addLeaves(NSMutableArray *out, NSUInteger base, MTLDataType t, MTLStructType *st, MTLArrayType *at,
                      NSString *what, NSString *field) {
    const char *scalar; int rows, cols; NSUInteger colStride;
    if (t == MTLDataTypeStruct && st) {
        for (MTLStructMember *m in st.members) {
            NSString *sub = field.length ? [field stringByAppendingFormat:@".%@", m.name] : m.name;
            addLeaves(out, base + m.offset, m.dataType, m.structType, m.arrayType, [what stringByAppendingFormat:@".%@", m.name], sub);
        }
    } else if (t == MTLDataTypeArray && at) {
        for (NSUInteger i = 0; i < at.arrayLength; i++)
            addLeaves(out, base + i * at.stride, at.elementType, at.elementStructType, at.elementArrayType, what, field);
    } else if (shape(t, &scalar, &rows, &cols, &colStride)) {
        NSUInteger sz = scalarSize(scalar);
        for (int c = 0; c < cols; c++)
            for (int r = 0; r < rows; r++)
                [out addObject:field.length ? @{@"off": @(base + c * colStride + r * sz), @"scalar": @(scalar), @"field": field}
                                            : @{@"off": @(base + c * colStride + r * sz), @"scalar": @(scalar)}];
    } else {
        [g_unsupported addObject:[NSString stringWithFormat:@"%@: data type %lu", what, (unsigned long)t]];
    }
}

// Buffers with their element layout; with `resources`, textures and samplers are listed as
// supported ({index, name, type, data, access}) instead of being "unsupported"; `graphics` also
// drops the vertex descriptor's own buffers.
static NSDictionary *reflectBindings(NSArray<id<MTLBinding>> *list, BOOL graphics, BOOL resources) {
    g_unsupported = [NSMutableArray array];
    NSMutableArray *buffers = [NSMutableArray array], *textures = [NSMutableArray array], *samplers = [NSMutableArray array],
        *threadgroups = [NSMutableArray array];
    for (id<MTLBinding> b in list) {
        if (graphics && [b.name hasPrefix:@"vertexBuffer."]) continue;  // the vertex descriptor's own buffers
        if (resources && b.type == MTLBindingTypeThreadgroupMemory) {
            id<MTLThreadgroupBinding> tb = (id<MTLThreadgroupBinding>)b;
            [threadgroups addObject:@{@"index": @(b.index), @"name": b.name, @"size": @(tb.threadgroupMemoryDataSize),
                @"alignment": @(tb.threadgroupMemoryAlignment)}];
            continue;
        }
        if (resources && b.type == MTLBindingTypeTexture) {
            id<MTLTextureBinding> tb = (id<MTLTextureBinding>)b;
            [textures addObject:@{@"index": @(b.index), @"name": b.name,
                @"type": tb.isDepthTexture ? @"depth" : textureTypeName(tb.textureType),
                @"data": dataName(tb.textureDataType),
                @"access": @[@"read", @"readwrite", @"write"][b.access]}];
            continue;
        }
        if (resources && b.type == MTLBindingTypeSampler) {
            [samplers addObject:@{@"index": @(b.index), @"name": b.name}];
            continue;
        }
        if (b.type != MTLBindingTypeBuffer) {
            [g_unsupported addObject:[NSString stringWithFormat:@"%@ (index %lu, binding type %ld)", b.name,
                (unsigned long)b.index, (long)b.type]];
            continue;
        }
        id<MTLBufferBinding> bb = (id<MTLBufferBinding>)b;
        NSMutableArray *leaves = [NSMutableArray array];
        NSUInteger stride;
        BOOL pointer = bb.bufferPointerType != nil;
        if (pointer) {
            MTLPointerType *p = bb.bufferPointerType;
            stride = p.elementIsArgumentBuffer ? 0 : p.dataSize;
            addLeaves(leaves, 0, p.elementType, p.elementStructType, p.elementArrayType, bb.name, @"");
        } else {
            stride = bb.bufferDataSize;
            addLeaves(leaves, 0, bb.bufferDataType, bb.bufferStructType, nil, bb.name, @"");
        }
        NSArray *access = @[@"read", @"readwrite", @"write"];
        [buffers addObject:@{@"index": @(bb.index), @"name": bb.name, @"access": access[bb.access],
            @"pointer": @(pointer), @"stride": @(stride), @"alignment": @(bb.bufferAlignment), @"leaves": leaves}];
    }
    return @{@"buffers": buffers, @"textures": textures, @"samplers": samplers, @"threadgroups": threadgroups,
        @"unsupported": g_unsupported};
}

static id<MTLLibrary> buildLibrary(id<MTLDevice> device, NSDictionary *m, NSError **error) {
    id<MTLLibrary> lib;
    if (m[@"metallib"]) {
        lib = [device newLibraryWithURL:[NSURL fileURLWithPath:m[@"metallib"][0]] error:error];
    } else {
        NSString *src = [NSString stringWithContentsOfFile:m[@"source"][0] encoding:NSUTF8StringEncoding error:nil];
        if (!src) die(@"cannot read the source");
        MTLCompileOptions *opts = [MTLCompileOptions new];
        opts.mathMode = MTLMathModeSafe;
        NSString *std = m[@"std"][0];
        if (m[@"contract"] && ![m[@"contract"][0] isEqual:@"on"] && ![m[@"contract"][0] isEqual:@"off"]) die(@"contract is on or off");
        if (std) {
            if ([std isEqual:@"2.4"]) opts.languageVersion = MTLLanguageVersion2_4;
            else if ([std isEqual:@"3.0"]) opts.languageVersion = MTLLanguageVersion3_0;
            else if ([std isEqual:@"3.1"]) opts.languageVersion = MTLLanguageVersion3_1;
            else if ([std isEqual:@"3.2"]) opts.languageVersion = MTLLanguageVersion3_2;
            else die(@"unknown std");
        }
        if (![m[@"contract"][0] isEqual:@"on"])
            src = [@"#pragma clang fp contract(off)\n#line 1\n" stringByAppendingString:src];
        lib = [device newLibraryWithSource:src options:opts error:error];
    }
    return lib;
}

// ---- graphics: one fixed draw into offscreen targets (see tools/diffcheck/graphics.py) ----

struct VFmt { const char *name; MTLVertexFormat format; NSUInteger size; };
static const VFmt kVertexFormats[] = {
    {"float", MTLVertexFormatFloat, 4}, {"float2", MTLVertexFormatFloat2, 8}, {"float3", MTLVertexFormatFloat3, 12},
    {"float4", MTLVertexFormatFloat4, 16}, {"int", MTLVertexFormatInt, 4}, {"int2", MTLVertexFormatInt2, 8},
    {"int3", MTLVertexFormatInt3, 12}, {"int4", MTLVertexFormatInt4, 16}, {"uint", MTLVertexFormatUInt, 4},
    {"uint2", MTLVertexFormatUInt2, 8}, {"uint3", MTLVertexFormatUInt3, 12}, {"uint4", MTLVertexFormatUInt4, 16},
    {"half", MTLVertexFormatHalf, 2}, {"half2", MTLVertexFormatHalf2, 4}, {"half3", MTLVertexFormatHalf3, 6},
    {"half4", MTLVertexFormatHalf4, 8},
};

struct CFmt { const char *name; MTLPixelFormat format; NSUInteger bytes; };
static const CFmt kColorFormats[] = {
    {"rgba32float", MTLPixelFormatRGBA32Float, 16}, {"rgba32sint", MTLPixelFormatRGBA32Sint, 16},
    {"rgba32uint", MTLPixelFormatRGBA32Uint, 16}, {"rgba16float", MTLPixelFormatRGBA16Float, 8},
    {"rg32float", MTLPixelFormatRG32Float, 8}, {"rg32sint", MTLPixelFormatRG32Sint, 8},
    {"rg32uint", MTLPixelFormatRG32Uint, 8}, {"rg16float", MTLPixelFormatRG16Float, 4},
    {"r32float", MTLPixelFormatR32Float, 4}, {"r32sint", MTLPixelFormatR32Sint, 4},
    {"r32uint", MTLPixelFormatR32Uint, 4}, {"r16float", MTLPixelFormatR16Float, 2},
};

static const VFmt &vertexFormat(NSString *name) {
    for (const VFmt &f : kVertexFormats) if ([name isEqual:@(f.name)]) return f;
    die([@"unknown vertex format " stringByAppendingString:name]);
    abort();
}

static const CFmt &colorFormat(NSString *name) {
    for (const CFmt &f : kColorFormats) if ([name isEqual:@(f.name)]) return f;
    die([@"unknown colour format " stringByAppendingString:name]);
    abort();
}

static NSData *slurp(NSString *path, NSUInteger expect);

struct TexelFmt { const char *type; NSUInteger components; MTLPixelFormat format; NSUInteger bytes; };
static const TexelFmt kTexelFormats[] = {
    {"float", 1, MTLPixelFormatR32Float, 4}, {"float", 2, MTLPixelFormatRG32Float, 8}, {"float", 4, MTLPixelFormatRGBA32Float, 16},
    {"half", 1, MTLPixelFormatR16Float, 2}, {"half", 2, MTLPixelFormatRG16Float, 4}, {"half", 4, MTLPixelFormatRGBA16Float, 8},
    {"int", 1, MTLPixelFormatR32Sint, 4}, {"int", 2, MTLPixelFormatRG32Sint, 8}, {"int", 4, MTLPixelFormatRGBA32Sint, 16},
    {"uint", 1, MTLPixelFormatR32Uint, 4}, {"uint", 2, MTLPixelFormatRG32Uint, 8}, {"uint", 4, MTLPixelFormatRGBA32Uint, 16},
    {"short", 1, MTLPixelFormatR16Sint, 2}, {"short", 2, MTLPixelFormatRG16Sint, 4}, {"short", 4, MTLPixelFormatRGBA16Sint, 8},
    {"ushort", 1, MTLPixelFormatR16Uint, 2}, {"ushort", 2, MTLPixelFormatRG16Uint, 4}, {"ushort", 4, MTLPixelFormatRGBA16Uint, 8},
};

static const TexelFmt &texelFormat(NSString *type, NSUInteger components) {
    for (const TexelFmt &f : kTexelFormats) if ([type isEqual:@(f.type)] && f.components == components) return f;
    die([NSString stringWithFormat:@"unknown texel buffer type %@ with %lu components", type, (unsigned long)components]);
    abort();
}

static void refuse(NSString *reason) {
    fprintf(stderr, "metalhost: unsupported: %s\n", reason.UTF8String);
    exit(2);
}

// The texture_buffer of a `texelbuffer` manifest line over a new shared buffer holding the input
// file: texels of 1, 2 or 4 components of T (a read of the missing ones gives 0, 0 and 1). The buffer is
// returned in `*backing` for the read-back after the dispatch.
static id<MTLTexture> texelBuffer(id<MTLDevice> device, NSArray *w, id<MTLBuffer> *backing) {
    const TexelFmt &f = texelFormat(w[5], [w[6] integerValue]);
    NSUInteger texels = [w[7] integerValue];
    NSData *d = slurp(w[8], texels * f.bytes);
    id<MTLBuffer> b = [device newBufferWithBytes:d.bytes length:d.length options:MTLResourceStorageModeShared];
    if (!b) refuse([NSString stringWithFormat:@"no %lu byte buffer", (unsigned long)d.length]);
    MTLTextureUsage usage = [w[4] isEqual:@"storage"] ? (MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite) : MTLTextureUsageShaderRead;
    MTLTextureDescriptor *td = [MTLTextureDescriptor textureBufferDescriptorWithPixelFormat:f.format width:texels
        resourceOptions:MTLResourceStorageModeShared usage:usage];
    id<MTLTexture> t = [b newTextureWithDescriptor:td offset:0 bytesPerRow:texels * f.bytes];
    if (!t) refuse([NSString stringWithFormat:@"no texture_buffer of %s over a %lu byte buffer", f.type, (unsigned long)d.length]);
    *backing = b;
    return t;
}

static NSData *slurp(NSString *path, NSUInteger expect) {
    NSData *d = [NSData dataWithContentsOfFile:path];
    if (!d || (expect != NSUIntegerMax && d.length != expect)) die([@"missing or wrong-sized input file " stringByAppendingString:path]);
    return d;
}

static id<MTLTexture> target(id<MTLDevice> device, MTLPixelFormat f, NSUInteger w, NSUInteger h) {
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:f width:w height:h mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget;
    td.storageMode = MTLStorageModePrivate;
    return [device newTextureWithDescriptor:td];
}

static MTLTextureType parseTextureType(NSString *kind) {
    if ([kind isEqual:@"1d"]) return MTLTextureType1D;
    if ([kind isEqual:@"1d_array"]) return MTLTextureType1DArray;
    if ([kind isEqual:@"2d"]) return MTLTextureType2D;
    if ([kind isEqual:@"2d_array"]) return MTLTextureType2DArray;
    if ([kind isEqual:@"3d"]) return MTLTextureType3D;
    if ([kind isEqual:@"cube"]) return MTLTextureTypeCube;
    if ([kind isEqual:@"cube_array"]) return MTLTextureTypeCubeArray;
    die([@"unknown texture type " stringByAppendingString:kind]);
    abort();
}

static NSUInteger textureSlices(MTLTextureType tt, NSUInteger layers) {
    return (tt == MTLTextureTypeCube || tt == MTLTextureTypeCubeArray) ? 6 * layers : layers;
}

struct TexSpec {
    NSString *kind, *type, *inFile, *outFile;
    NSUInteger index, w, h, d, layers, mips;
    BOOL writable;
};

static TexSpec parseTexSpec(NSArray *w) {
    TexSpec spec;
    if ([w[0] isEqual:@"image"]) {
        if (w.count != 12 && w.count != 13) die(@"bad image manifest line");
        BOOL wr = [w[2] isEqual:@"write"] || [w[2] isEqual:@"readwrite"];
        if (!wr && ![w[2] isEqual:@"sample"] && ![w[2] isEqual:@"read"]) die([@"bad image mode " stringByAppendingString:w[2]]);
        NSUInteger mips = 1;
        NSString *out = nil;
        if (wr) {
            if (w.count != 13) die(@"image output file mismatch");
            out = w[12];
        } else if (w.count == 13) {
            if (![w[12] hasPrefix:@"mips="]) die(@"bad image manifest line");
            mips = parseU32([w[12] substringFromIndex:5], @"image mips");
        }
        spec = {w[1], w[10], w[11], out,
                (NSUInteger)[w[3] integerValue], (NSUInteger)[w[6] integerValue], (NSUInteger)[w[7] integerValue],
                (NSUInteger)[w[8] integerValue], (NSUInteger)[w[9] integerValue], mips, wr};
    } else {
        if (w.count < 7) die(@"bad texture manifest line");
        spec = {@"2d", w.count >= 8 ? w[7] : @"float", w[6], w.count >= 10 ? w[9] : nil,
                (NSUInteger)[w[1] integerValue], (NSUInteger)[w[4] integerValue], (NSUInteger)[w[5] integerValue],
                1, 1, 1, w.count >= 10};
    }
    if (!spec.w || !spec.h || !spec.d || !spec.layers || !spec.mips || spec.mips > 16) die(@"bad texture dimensions");
    return spec;
}

struct TexLayout {
    MTLTextureType tt;
    const TexelFmt &f;
    NSUInteger slices, sliceBytes, rowBytes, imgBytes;
    MTLRegion reg;
};

static NSUInteger mipDim(NSUInteger base, NSUInteger lv) {
    NSUInteger v = lv < 32 ? (base >> lv) : 0;
    return v ? v : 1;
}

static TexLayout texLayout(const TexSpec &spec, NSUInteger lv = 0) {
    MTLTextureType tt = parseTextureType(spec.kind);
    const TexelFmt &f = texelFormat(spec.type, 4);
    NSUInteger mw = mipDim(spec.w, lv);
    NSUInteger mh = (tt == MTLTextureType1D || tt == MTLTextureType1DArray) ? 1 : mipDim(spec.h, lv);
    NSUInteger md = (tt == MTLTextureType3D) ? mipDim(spec.d, lv) : 1;
    NSUInteger rowBytes = (tt == MTLTextureType1D || tt == MTLTextureType1DArray) ? 0 : mw * f.bytes;
    NSUInteger imgBytes = (tt == MTLTextureType3D) ? mw * mh * f.bytes : 0;
    return {tt, f, textureSlices(tt, spec.layers), mw * mh * md * f.bytes, rowBytes, imgBytes,
            MTLRegionMake3D(0, 0, 0, mw, mh, md)};
}

// The texture of a `texture`, `storagetexture` or `image` manifest line.
static id<MTLTexture> sourceTexture(id<MTLDevice> device, const TexSpec &spec) {
    TexLayout baseLay = texLayout(spec, 0);
    MTLTextureDescriptor *td = [MTLTextureDescriptor new];
    td.textureType = baseLay.tt; td.pixelFormat = baseLay.f.format;
    td.width = spec.w; td.height = spec.h; td.depth = spec.d;
    td.mipmapLevelCount = spec.mips; td.arrayLength = spec.layers;
    td.usage = spec.writable ? (MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite) : MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModeShared;
    id<MTLTexture> t = [device newTextureWithDescriptor:td];
    if (!t) refuse([NSString stringWithFormat:@"no %@ texture of %s", spec.kind, baseLay.f.type]);
    NSUInteger totalBytes = 0;
    for (NSUInteger lv = 0; lv < spec.mips; lv++) {
        TexLayout lay = texLayout(spec, lv);
        totalBytes += lay.slices * lay.sliceBytes;
    }
    NSData *texels = slurp(spec.inFile, totalBytes);
    NSUInteger off = 0;
    for (NSUInteger lv = 0; lv < spec.mips; lv++) {
        TexLayout lay = texLayout(spec, lv);
        for (NSUInteger s = 0; s < lay.slices; s++) {
            [t replaceRegion:lay.reg mipmapLevel:lv slice:s withBytes:(const uint8_t *)texels.bytes + off
                 bytesPerRow:lay.rowBytes bytesPerImage:lay.imgBytes];
            off += lay.sliceBytes;
        }
    }
    return t;
}

static void writeTextureOutput(id<MTLTexture> t, const TexSpec &spec) {
    TexLayout lay = texLayout(spec);
    NSMutableData *o = [NSMutableData dataWithLength:lay.slices * lay.sliceBytes];
    for (NSUInteger s = 0; s < lay.slices; s++)
        [t getBytes:(uint8_t *)o.mutableBytes + s * lay.sliceBytes bytesPerRow:lay.rowBytes bytesPerImage:lay.imgBytes
         fromRegion:lay.reg mipmapLevel:0 slice:s];
    if (![o writeToFile:spec.outFile atomically:YES]) die(@"cannot write a texture output file");
}

// The address mode follows the sampler index (0 clamp to edge, 1 repeat, 2 mirrored repeat), so a
// swapped sampler binding changes what a coordinate outside [0, 1] reads; vkhost does the same.
static id<MTLSamplerState> nearestSampler(id<MTLDevice> device, NSUInteger index) {
    MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
    sd.minFilter = sd.magFilter = MTLSamplerMinMagFilterNearest;
    sd.mipFilter = MTLSamplerMipFilterNearest;
    sd.sAddressMode = sd.tAddressMode = sd.rAddressMode = index % 3 == 0 ? MTLSamplerAddressModeClampToEdge
        : index % 3 == 1 ? MTLSamplerAddressModeRepeat : MTLSamplerAddressModeMirrorRepeat;
    return [device newSamplerStateWithDescriptor:sd];
}

// Reflects the render pipeline (draw == NO) or draws and writes every attachment to its file.
// Manifest lines: vertex, fragment, target W H, draw V I [BV BI], index N FILE, attr LOC FMT [FILE], color I FMT [C0..C3 FILE],
// depth [CLEAR FILE], gbuffer STAGE INDEX SET MEMBER BYTES FILE [OUTFILE], texture|storagetexture INDEX SET BINDING W H FILE [TYPE],
// image KIND ACCESS INDEX SET BINDING W H D LAYERS TYPE FILE [mips=N], sampler INDEX SET BINDING (set 0 is vertex, 1 is fragment).
// Vertex buffer slot for attribute n is 30 - n.
static int drawMain(id<MTLDevice> device, id<MTLLibrary> lib, NSDictionary *m, NSDictionary<NSString *, NSMutableArray *> *lists, BOOL draw) {
    NSUInteger width = [m[@"target"][0] integerValue], height = [m[@"target"][1] integerValue];
    if (!width || !height) die(@"manifest has no target size");
    id<MTLFunction> vfn = [lib newFunctionWithName:m[@"vertex"][0]], ffn = [lib newFunctionWithName:m[@"fragment"][0]];
    if (!vfn || !ffn) die(@"no such entry point in the Apple library");
    MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = vfn;
    pd.fragmentFunction = ffn;
    if (lists[@"attr"].count) {
        MTLVertexDescriptor *vd = [MTLVertexDescriptor vertexDescriptor];
        for (NSArray *w in lists[@"attr"]) {
            NSUInteger loc = [w[1] integerValue];
            const VFmt &f = vertexFormat(w[2]);
            vd.attributes[loc].format = f.format;
            vd.attributes[loc].offset = 0;
            vd.attributes[loc].bufferIndex = 30 - loc;
            vd.layouts[30 - loc].stride = f.size;
            vd.layouts[30 - loc].stepFunction = MTLVertexStepFunctionPerVertex;
        }
        pd.vertexDescriptor = vd;
    }
    for (NSArray *w in lists[@"color"]) pd.colorAttachments[[w[1] integerValue]].pixelFormat = colorFormat(w[2]).format;
    if (m[@"depth"]) pd.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    NSError *err = nil;
    MTLRenderPipelineReflection *refl = nil;
    id<MTLRenderPipelineState> pso = [device newRenderPipelineStateWithDescriptor:pd
        options:MTLPipelineOptionBindingInfo | MTLPipelineOptionBufferTypeInfo reflection:&refl error:&err];
    if (!pso) die([@"pipeline failed: " stringByAppendingString:err.localizedDescription ?: @"?"]);
    if (!draw) {
        // The stage_in attributes the Metal source declares, independent of the vertex descriptor built from mslc's types.
        NSMutableArray *attrs = [NSMutableArray array];
        for (MTLVertexAttribute *a in vfn.vertexAttributes) {
            const char *scalar; int rows, cols; NSUInteger colStride;
            if (shape(a.attributeType, &scalar, &rows, &cols, &colStride) && cols == 1)
                [attrs addObject:@{@"index": @(a.attributeIndex), @"name": a.name, @"scalar": @(scalar), @"comps": @(rows)}];
            else
                [attrs addObject:@{@"index": @(a.attributeIndex), @"name": a.name, @"scalar": @"other", @"comps": @0}];
        }
        NSDictionary *doc = @{@"device": device.name, @"vertex": reflectBindings(refl.vertexBindings, YES, YES),
            @"fragment": reflectBindings(refl.fragmentBindings, YES, YES), @"vertex_attributes": attrs};
        NSData *j = [NSJSONSerialization dataWithJSONObject:doc options:NSJSONWritingSortedKeys error:nil];
        if (!m[@"reflection"] || ![j writeToFile:m[@"reflection"][0] atomically:YES]) die(@"cannot write the reflection");
        return 0;
    }

    NSArray *drawArgs = m[@"draw"];
    if (drawArgs.count != 2 && drawArgs.count != 4) die(@"bad draw line in manifest");
    NSUInteger vertices = parseU32(drawArgs[0], @"draw"), instances = parseU32(drawArgs[1], @"draw");
    NSUInteger baseVert = drawArgs.count == 4 ? parseU32(drawArgs[2], @"draw") : 0;
    NSUInteger baseInst = drawArgs.count == 4 ? parseU32(drawArgs[3], @"draw") : 0;
    if (!vertices || !instances || baseVert > INT32_MAX) die(@"bad draw line in manifest");
    id<MTLCommandQueue> q = [device newCommandQueue];
    id<MTLCommandBuffer> cb = [q commandBuffer];
    MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
    NSMutableArray<id<MTLTexture>> *colorTex = [NSMutableArray array];
    for (NSArray *w in lists[@"color"]) {
        id<MTLTexture> t = target(device, colorFormat(w[2]).format, width, height);
        MTLRenderPassColorAttachmentDescriptor *ca = rp.colorAttachments[[w[1] integerValue]];
        ca.texture = t;
        ca.loadAction = MTLLoadActionClear;
        ca.storeAction = MTLStoreActionStore;
        ca.clearColor = MTLClearColorMake([w[3] doubleValue], [w[4] doubleValue], [w[5] doubleValue], [w[6] doubleValue]);
        [colorTex addObject:t];
    }
    id<MTLTexture> depthTex = nil;
    if (m[@"depth"]) {
        depthTex = target(device, MTLPixelFormatDepth32Float, width, height);
        rp.depthAttachment.texture = depthTex;
        rp.depthAttachment.loadAction = MTLLoadActionClear;
        rp.depthAttachment.storeAction = MTLStoreActionStore;
        rp.depthAttachment.clearDepth = [m[@"depth"][0] doubleValue];
    }
    id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rp];
    [enc setRenderPipelineState:pso];
    [enc setCullMode:MTLCullModeNone];
    [enc setViewport:(MTLViewport){0, 0, (double)width, (double)height, 0, 1}];
    if (depthTex) {
        MTLDepthStencilDescriptor *dd = [MTLDepthStencilDescriptor new];
        dd.depthCompareFunction = MTLCompareFunctionAlways;
        dd.depthWriteEnabled = YES;
        [enc setDepthStencilState:[device newDepthStencilStateWithDescriptor:dd]];
    }
    NSMutableArray *keep = [NSMutableArray array];
    for (NSArray *w in lists[@"attr"]) {
        NSData *d = slurp(w[3], (baseVert + vertices) * vertexFormat(w[2]).size);
        id<MTLBuffer> b = [device newBufferWithBytes:d.bytes length:d.length ?: 4 options:MTLResourceStorageModeShared];
        [enc setVertexBuffer:b offset:0 atIndex:30 - [w[1] integerValue]];
        [keep addObject:b];
    }
    NSMutableArray<id<MTLBuffer>> *goutBufs = [NSMutableArray array];
    NSMutableArray<NSString *> *goutFiles = [NSMutableArray array];
    for (NSArray *w in lists[@"gbuffer"]) {
        if (w.count != 7 && w.count != 8) die(@"bad gbuffer line in draw manifest");
        NSData *d = slurp(w[6], [w[5] integerValue]);
        id<MTLBuffer> b = [device newBufferWithBytes:d.bytes length:d.length ?: 4 options:MTLResourceStorageModeShared];
        if ([w[1] isEqual:@"vertex"]) [enc setVertexBuffer:b offset:0 atIndex:[w[2] integerValue]];
        else if ([w[1] isEqual:@"fragment"]) [enc setFragmentBuffer:b offset:0 atIndex:[w[2] integerValue]];
        else die(@"unknown buffer stage in draw manifest");
        [keep addObject:b];
        if (w.count == 8) { [goutBufs addObject:b]; [goutFiles addObject:w[7]]; }
    }
    for (NSString *key in @[@"texture", @"storagetexture", @"image"]) {
        BOOL isImage = [key isEqual:@"image"];
        for (NSArray *w in lists[key]) {
            if (isImage ? (w.count != 12 && w.count != 13) : (w.count != 7 && w.count != 8)) die(@"bad texture line in draw manifest");
            NSString *setStr = isImage ? w[4] : w[2];
            if (![setStr isEqual:@"0"] && ![setStr isEqual:@"1"]) die(@"unknown texture stage set in draw manifest");
            TexSpec spec = parseTexSpec(w);
            if (spec.writable) die(@"writable texture in draw manifest");
            id<MTLTexture> t = sourceTexture(device, spec);
            if ([setStr isEqual:@"0"]) [enc setVertexTexture:t atIndex:spec.index];
            else [enc setFragmentTexture:t atIndex:spec.index];
            [keep addObject:t];
        }
    }
    for (NSArray *w in lists[@"sampler"]) {
        if (w.count != 4) die(@"bad sampler line in draw manifest");
        if (![w[2] isEqual:@"0"] && ![w[2] isEqual:@"1"]) die(@"unknown sampler stage set in draw manifest");
        id<MTLSamplerState> s = nearestSampler(device, [w[1] integerValue]);
        NSUInteger idx = [w[1] integerValue];
        if ([w[2] isEqual:@"0"]) [enc setVertexSamplerState:s atIndex:idx];
        else [enc setFragmentSamplerState:s atIndex:idx];
        [keep addObject:s];
    }
    NSArray *idxArgs = m[@"index"];
    if (idxArgs) {
        if (idxArgs.count != 2) die(@"bad index line in manifest");
        NSUInteger indexCount = parseU32(idxArgs[0], @"index");
        if (!indexCount) die(@"bad index line in manifest");
        NSData *idxData = slurp(idxArgs[1], indexCount * 4);
        id<MTLBuffer> ib = [device newBufferWithBytes:idxData.bytes length:idxData.length options:MTLResourceStorageModeShared];
        [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:indexCount indexType:MTLIndexTypeUInt32
            indexBuffer:ib indexBufferOffset:0 instanceCount:instances baseVertex:(NSInteger)baseVert baseInstance:baseInst];
    } else {
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:baseVert vertexCount:vertices instanceCount:instances baseInstance:baseInst];
    }
    [enc endEncoding];

    // Private targets are copied to shared buffers; the buffers are what the host reads.
    NSMutableArray<id<MTLBuffer>> *outs = [NSMutableArray array];
    id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
    MTLSize size = MTLSizeMake(width, height, 1);
    for (NSUInteger i = 0; i < colorTex.count; i++) {
        NSUInteger bpp = colorFormat(lists[@"color"][i][2]).bytes;
        id<MTLBuffer> b = [device newBufferWithLength:width * height * bpp options:MTLResourceStorageModeShared];
        [blit copyFromTexture:colorTex[i] sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:size
            toBuffer:b destinationOffset:0 destinationBytesPerRow:width * bpp destinationBytesPerImage:width * height * bpp];
        [outs addObject:b];
    }
    id<MTLBuffer> depthOut = nil;
    if (depthTex) {
        depthOut = [device newBufferWithLength:width * height * 4 options:MTLResourceStorageModeShared];
        [blit copyFromTexture:depthTex sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:size
            toBuffer:depthOut destinationOffset:0 destinationBytesPerRow:width * 4 destinationBytesPerImage:width * height * 4
            options:MTLBlitOptionDepthFromDepthStencil];
    }
    [blit endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    if (cb.status != MTLCommandBufferStatusCompleted) die([@"command buffer failed: " stringByAppendingString:cb.error.localizedDescription ?: @"?"]);
    for (NSUInteger i = 0; i < outs.count; i++)
        if (![[NSData dataWithBytes:outs[i].contents length:outs[i].length] writeToFile:lists[@"color"][i][7] atomically:YES]) die(@"cannot write a colour file");
    if (depthOut && ![[NSData dataWithBytes:depthOut.contents length:depthOut.length] writeToFile:m[@"depth"][1] atomically:YES]) die(@"cannot write the depth file");
    for (NSUInteger i = 0; i < goutBufs.count; i++)
        if (![[NSData dataWithBytes:goutBufs[i].contents length:goutBufs[i].length] writeToFile:goutFiles[i] atomically:YES]) die(@"cannot write a buffer output file");
    printf("device: %s\n", device.name.UTF8String);
    return 0;
}

int main(int argc, char **argv) {
    @autoreleasepool {
        if (argc != 2) die(@"usage: metalhost manifest");
        NSMutableDictionary *m = [NSMutableDictionary dictionary];
        NSMutableArray *bufSpecs = [NSMutableArray array];
        NSMutableDictionary<NSString *, NSMutableArray *> *lists = [NSMutableDictionary dictionary];
        NSSet *multi = [NSSet setWithArray:@[@"attr", @"color", @"gbuffer", @"texture", @"storagetexture", @"image", @"sampler", @"threadgroup", @"texelbuffer"]];
        NSString *text = [NSString stringWithContentsOfFile:@(argv[1]) encoding:NSUTF8StringEncoding error:nil];
        if (!text) die(@"cannot read manifest");
        for (NSString *line in [text componentsSeparatedByString:@"\n"]) {
            NSArray *w = [line componentsSeparatedByString:@" "];
            if (line.length == 0) continue;
            if ([w[0] isEqual:@"buffer"] && w.count == 5) [bufSpecs addObject:w];
            else if ([multi containsObject:w[0]]) {
                if (!lists[w[0]]) lists[w[0]] = [NSMutableArray array];
                [lists[w[0]] addObject:w];
            } else if (w.count >= 2 || [w[0] isEqual:@"depth"]) m[w[0]] = [w subarrayWithRange:NSMakeRange(1, w.count - 1)];
            else die([@"bad manifest line: " stringByAppendingString:line]);
        }
        BOOL run = [m[@"mode"][0] isEqual:@"run"];
        BOOL gfx = [m[@"mode"][0] isEqual:@"draw"] || [m[@"mode"][0] isEqual:@"reflect-gfx"];
        NSString *entry = m[@"entry"][0];
        if (!entry && !gfx) die(@"manifest has no entry");

        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) die(@"no Metal device");
        NSError *err = nil;
        id<MTLLibrary> lib = buildLibrary(device, m, &err);
        if (!lib) die([@"Apple compile failed: " stringByAppendingString:err.localizedDescription ?: @"?"]);
        if (gfx) return drawMain(device, lib, m, lists, [m[@"mode"][0] isEqual:@"draw"]);
        id<MTLFunction> fn = [lib newFunctionWithName:entry];
        if (!fn) die(@"no such entry point in the Apple library");

        MTLComputePipelineDescriptor *pd = [MTLComputePipelineDescriptor new];
        pd.computeFunction = fn;
        MTLComputePipelineReflection *refl = nil;
        id<MTLComputePipelineState> pso = [device newComputePipelineStateWithDescriptor:pd
            options:MTLPipelineOptionBindingInfo | MTLPipelineOptionBufferTypeInfo reflection:&refl error:&err];
        if (!pso) die([@"pipeline failed: " stringByAppendingString:err.localizedDescription ?: @"?"]);

        NSDictionary *bindings = reflectBindings(refl.bindings, NO, YES);
        NSArray *buffers = bindings[@"buffers"];
        NSDictionary *doc = @{@"device": device.name, @"buffers": buffers, @"textures": bindings[@"textures"], @"samplers": bindings[@"samplers"],
            @"threadgroups": bindings[@"threadgroups"], @"unsupported": bindings[@"unsupported"],
            @"max_threads": @(pso.maxTotalThreadsPerThreadgroup), @"execution_width": @(pso.threadExecutionWidth)};
        if (m[@"reflection"]) {
            NSData *j = [NSJSONSerialization dataWithJSONObject:doc options:NSJSONWritingSortedKeys error:nil];
            [j writeToFile:m[@"reflection"][0] atomically:YES];
        }
        if (!run) return 0;

        NSArray *lc = m[@"local"], *gc = m[@"groups"];
        if (lc.count != 3 || gc.count != 3) die(@"run mode needs local and groups");
        id<MTLCommandQueue> q = [device newCommandQueue];
        id<MTLCommandBuffer> cb = [q commandBuffer];
        id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
        [enc setComputePipelineState:pso];
        NSMutableArray<id<MTLBuffer>> *bufs = [NSMutableArray array];
        NSMutableArray *keep = [NSMutableArray array];
        std::vector<TexSpec> texSpecs;
        for (NSString *key in @[@"texture", @"storagetexture", @"image"])
            for (NSArray *w in lists[key]) texSpecs.push_back(parseTexSpec(w));
        NSMutableArray<id<MTLTexture>> *texs = [NSMutableArray array];
        for (const TexSpec &spec : texSpecs) {
            id<MTLTexture> t = sourceTexture(device, spec);
            [enc setTexture:t atIndex:spec.index];
            [texs addObject:t];
        }
        for (NSArray *w in lists[@"threadgroup"]) [enc setThreadgroupMemoryLength:[w[2] integerValue] atIndex:[w[1] integerValue]];
        for (NSArray *w in lists[@"sampler"]) [enc setSamplerState:nearestSampler(device, [w[1] integerValue]) atIndex:[w[1] integerValue]];
        NSMutableArray<id<MTLBuffer>> *texelBacking = [NSMutableArray array];
        for (NSArray *w in lists[@"texelbuffer"]) {
            id<MTLBuffer> backing;
            id<MTLTexture> t = texelBuffer(device, w, &backing);
            [enc setTexture:t atIndex:[w[1] integerValue]];
            [keep addObject:t];
            [texelBacking addObject:backing];
        }
        for (NSArray *w in bufSpecs) {
            NSUInteger idx = [w[1] integerValue], n = [w[2] integerValue];
            NSData *d = [NSData dataWithContentsOfFile:w[3]];
            if (d.length != n) die(@"input file size differs from the manifest");
            id<MTLBuffer> b = [device newBufferWithBytes:d.bytes length:n ? n : 4 options:MTLResourceStorageModeShared];
            [enc setBuffer:b offset:0 atIndex:idx];
            [bufs addObject:b];
        }
        [enc dispatchThreadgroups:MTLSizeMake([gc[0] integerValue], [gc[1] integerValue], [gc[2] integerValue])
            threadsPerThreadgroup:MTLSizeMake([lc[0] integerValue], [lc[1] integerValue], [lc[2] integerValue])];
        [enc endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
        if (cb.status != MTLCommandBufferStatusCompleted) die([@"command buffer failed: " stringByAppendingString:cb.error.localizedDescription ?: @"?"]);
        for (NSUInteger i = 0; i < bufSpecs.count; i++) {
            NSUInteger n = [bufSpecs[i][2] integerValue];
            NSData *o = [NSData dataWithBytes:bufs[i].contents length:n];
            if (![o writeToFile:bufSpecs[i][4] atomically:YES]) die(@"cannot write an output file");
        }
        for (size_t i = 0; i < texSpecs.size(); i++)
            if (texSpecs[i].writable) writeTextureOutput(texs[i], texSpecs[i]);
        for (NSUInteger i = 0; i < texelBacking.count; i++)
            if (![[NSData dataWithBytes:texelBacking[i].contents length:texelBacking[i].length] writeToFile:lists[@"texelbuffer"][i][9] atomically:YES])
                die(@"cannot write a texel buffer output file");
        printf("device: %s\n", device.name.UTF8String);
    }
    return 0;
}
