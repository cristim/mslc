// Vulkan side of diffcheck: runs one mslc compute entry point on whatever ICD the
// loader selects (lavapipe: set VK_ICD_FILENAMES) and writes every buffer back.
//
// Binding model (mslc's, see sema.cpp): the kernel's device/constant buffers are not
// descriptors. Binding 0 of the entry point's set is a uniform block whose members
// are the buffers' PhysicalStorageBuffer64 addresses, in parameter order ("member"
// in the reflection). Local size comes from spec constants 0, 1, 2 (LocalSizeId).
//
// Manifest, one directive per line:
//   spv <file.spv>          entry <name>          set <descriptor set of the block>
//   local X Y Z             groups X Y Z
//   buffer <member> <bytes> <input file> <output file>
//   texture|storagetexture <metal index> <set> <binding> <w> <h> <rgba file> [<type> [write|readwrite <output file>]]
//                                                                       (at most 8)
//   image <kind> <access> <metal index> <set> <binding> <w> <h> <d> <layers> <type> <in> [<out>]
//                                                                       (1d, 1d_array, 2d_array, 3d, cube, cube_array)
//   esampler <set> <binding> <S> <T> <R> <linear 0|1>   (kernel mode: an embedded sampler, state in VkSamplerAddressMode values)
//   sampler <metal index> <set> <binding>                               (at most 8)
//   spec <constant id> <value>   a specialization constant besides the local size (mslc's threadgroup
//                                memory arguments: the element count of their Workgroup array; at most 8)
//   texelbuffer <metal index> <set> <binding> uniform|storage <type> <components 1|2|4> <texels> <input file> <output file>
//                                                                       (at most 8)
// A texture is an RGBA sampled or storage image of <type> (default float; with `write|readwrite`, written back to
// the output file after the dispatch) and a sampler is nearest and normalised, its address mode picked by the Metal index (0 clamp to edge, 1 repeat,
// 2 mirrored repeat), as in metalhost. A texel buffer is a VkBufferView of <texels> texels of <components> (1, 2 or 4) components of <type>
// (float half int uint short ushort: R32_SFLOAT, R32G32_SFLOAT, R32G32B32A32_SFLOAT, R16..., ..._SINT, ..._UINT) over a
// buffer filled from the input file, a UNIFORM_TEXEL_BUFFER descriptor (sampled) or a STORAGE_TEXEL_BUFFER one,
// and its bytes are written to the output file after the dispatch. A device without the format feature or
// the format-less storage image read and write features exits 2 with "unsupported: <reason>", which the
// driver reports as a skip. Exit 0 on success, 2 on any Vulkan or file error (never a mismatch: this host
// compares nothing). The device name goes to stdout as "device: ...".
//
// Details taken from the audited PR #148 harness: the address block's memory is sized
// from vkGetBufferMemoryRequirements, a shader-write to host-read barrier precedes the
// read-back, and maintenance4 is enabled for LocalSizeId.
#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHK(x) do { VkResult r_ = (x); if (r_ != VK_SUCCESS) die("%s failed: %d", #x, (int)r_); } while (0)
#define MAXBUF 32
#define MAXSPEC 8

static void die(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
#include <stdarg.h>
static void die(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); fprintf(stderr, "vkhost: "); vfprintf(stderr, fmt, ap); fputc('\n', stderr);
    va_end(ap); exit(2);
}

// A feature or format this device lacks: the driver reports a skip with the reason, not an error.
static void refuse(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
static void refuse(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); fprintf(stderr, "vkhost: unsupported: "); vfprintf(stderr, fmt, ap); fputc('\n', stderr);
    va_end(ap); exit(2);
}
static VkDevice dev;
static VkPhysicalDevice gpd;
static VkPhysicalDeviceFeatures gfeatures;
static VkPhysicalDeviceMemoryProperties mp;

static uint32_t memType(uint32_t bits) {
    VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) return i;
    die("no host-visible coherent memory type");
}

typedef struct { VkBuffer buf; VkDeviceMemory mem; VkDeviceSize size; } Buf;

static Buf makeBuf(VkDeviceSize size, VkBufferUsageFlags usage, int addressable) {
    Buf b = {0};
    VkBufferCreateInfo ci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ci.size = size ? size : 4; ci.usage = usage;
    CHK(vkCreateBuffer(dev, &ci, NULL, &b.buf));
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(dev, b.buf, &req);
    VkMemoryAllocateFlagsInfo fl = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
    fl.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.pNext = addressable ? &fl : NULL;
    ai.allocationSize = req.size; ai.memoryTypeIndex = memType(req.memoryTypeBits);
    CHK(vkAllocateMemory(dev, &ai, NULL, &b.mem));
    CHK(vkBindBufferMemory(dev, b.buf, b.mem, 0));
    b.size = size;
    return b;
}

static void *slurp(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb");
    if (!f) die("cannot open %s", path);
    fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    void *p = malloc(l ? l : 1);
    if (fread(p, 1, l, f) != (size_t)l) die("short read of %s", path);
    fclose(f); *n = (size_t)l; return p;
}

// Instance, device (with every feature the device offers) and the first queue family with
// `queueFlag`; sets dev and mp. Prints "device: ..." for the driver's lavapipe check.
static uint32_t initDevice(VkQueueFlags queueFlag) {
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    VkInstance inst;
    CHK(vkCreateInstance(&ici, NULL, &inst));
    VkPhysicalDevice pd; uint32_t npd = 1;
    VkResult er = vkEnumeratePhysicalDevices(inst, &npd, &pd);
    if ((er != VK_SUCCESS && er != VK_INCOMPLETE) || npd == 0) die("no Vulkan device (is VK_ICD_FILENAMES set to lavapipe?)");
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(pd, &props);
    printf("device: %s\n", props.deviceName);
    vkGetPhysicalDeviceMemoryProperties(pd, &mp);

    uint32_t qf = UINT32_MAX, nq = 8; VkQueueFamilyProperties qfp[8];
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &nq, qfp);
    for (uint32_t i = 0; i < nq; i++) if (qfp[i].queueFlags & queueFlag) { qf = i; break; }
    if (qf == UINT32_MAX) die("no queue with the needed capability");

    // Enable everything the device offers: the module's capabilities (Int64, Float16,
    // 8/16-bit storage, ...) depend on the kernel, and this host does not parse them.
    VkPhysicalDeviceVulkan13Features v13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceVulkan12Features v12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan11Features v11 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    VkPhysicalDeviceFeatures2 f2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    f2.pNext = &v11; v11.pNext = &v12; v12.pNext = &v13;
    vkGetPhysicalDeviceFeatures2(pd, &f2);
    gpd = pd; gfeatures = f2.features;
    if (!v12.bufferDeviceAddress) die("device lacks bufferDeviceAddress");
    if (!v13.maintenance4) die("device lacks maintenance4 (LocalSizeId)");
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = qf; qci.queueCount = 1; qci.pQueuePriorities = &prio;
    VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.pNext = &f2; dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci;
    CHK(vkCreateDevice(pd, &dci, NULL, &dev));
    return qf;
}

// ---- textures and samplers, shared by the compute and the draw path ----

#define MAXTEX 8
#define MAXTB 8
typedef enum { TEX_1D, TEX_1D_ARRAY, TEX_2D, TEX_2D_ARRAY, TEX_3D, TEX_CUBE, TEX_CUBE_ARRAY } TexKind;
static VkFormat texelFormat(const char *type, unsigned components, unsigned *bytes);
typedef struct { VkImage img; VkDeviceMemory mem; VkImageView view; } Img;
static TexKind parseTexKind(const char *s);
static unsigned texSlices(TexKind kind, unsigned layers);
static VkImageType texImageType(TexKind kind);
static VkImageCreateFlags texCreateFlags(TexKind kind);
static VkImageUsageFlags texUsage(int storage);
static Img makeImage(TexKind kind, VkFormat f, unsigned w, unsigned h, unsigned d, unsigned layers,
                     VkImageUsageFlags usage, VkImageAspectFlags aspect);
static VkImageMemoryBarrier imgBarrier(VkImage img, VkImageAspectFlags aspect, unsigned layers,
                                       VkImageLayout from, VkImageLayout to, VkAccessFlags src, VkAccessFlags dst);
static void fillBuf(Buf b, const void *data, size_t n);

// Nearest, normalised; the address mode is picked by the Metal sampler index, as in metalhost, so a
// swapped sampler binding changes what a coordinate outside [0, 1] reads: 0 clamp to edge,
// 1 repeat, 2 mirrored repeat.
// An embedded (`constexpr`) sampler is listed with its own state, `esampler SET BINDING S T R FILTER`
// (the three VkSamplerAddressMode values, FILTER 0 nearest or 1 linear), because Apple has it in the shader.
static VkSampler makeSampler(VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w, VkFilter filter) {
    VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = sci.minFilter = filter; sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = u; sci.addressModeV = v; sci.addressModeW = w;
    sci.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    VkSampler sampler; CHK(vkCreateSampler(dev, &sci, NULL, &sampler));
    return sampler;
}

static VkSampler makeNearestSampler(unsigned index) {
    VkSamplerAddressMode m = index % 3 == 0 ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
        : index % 3 == 1 ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    return makeSampler(m, m, m, VK_FILTER_NEAREST);
}

// The image and the staging buffer holding the file's RGBA texels (`storage` 0 sampled, 1 read, 2 write, 3 readwrite).
static Img loadTexture(TexKind kind, const char *file, const char *type, unsigned w, unsigned h, unsigned d, unsigned layers,
                       int storage, Buf *stage, unsigned *bpp) {
    VkFormat fmt = texelFormat(type, 4, bpp);
    Img t = makeImage(kind, fmt, w, h, d, layers, texUsage(storage), VK_IMAGE_ASPECT_COLOR_BIT);
    size_t n, expect = (size_t)w * h * d * texSlices(kind, layers) * *bpp;
    void *data = slurp(file, &n);
    if (n != expect) die("%s has the wrong size", file);
    *stage = makeBuf(n, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 0); fillBuf(*stage, data, n); free(data);
    return t;
}

// Copies the staging buffer into the image and makes it accessible to `consumer`.
static void recordTextureUpload(VkCommandBuffer cmd, Img t, Buf stage, TexKind kind, unsigned w, unsigned h, unsigned d,
                                unsigned layers, int storage, VkPipelineStageFlags consumer) {
    unsigned totalLayers = texSlices(kind, layers);
    VkImageMemoryBarrier to = imgBarrier(t.img, VK_IMAGE_ASPECT_COLOR_BIT, totalLayers, VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &to);
    VkBufferImageCopy bc = {0, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, totalLayers}, {0, 0, 0}, {w, h, d}};
    vkCmdCopyBufferToImage(cmd, stage.buf, t.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bc);
    VkAccessFlags dst = (storage != 2 ? VK_ACCESS_SHADER_READ_BIT : 0) | (storage >= 2 ? VK_ACCESS_SHADER_WRITE_BIT : 0);
    VkImageMemoryBarrier rd = imgBarrier(t.img, VK_IMAGE_ASPECT_COLOR_BIT, totalLayers, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         storage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                         VK_ACCESS_TRANSFER_WRITE_BIT, dst);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, consumer, 0, 0, NULL, 0, NULL, 1, &rd);
}

static void checkTextureSupport(TexKind kind, const char *type, int storage, unsigned *bpp) {
    VkFormat fmt = texelFormat(type, 4, bpp);
    VkFormatProperties fp; vkGetPhysicalDeviceFormatProperties(gpd, fmt, &fp);
    VkFormatFeatureFlags need = storage ? VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT : VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    if (!(fp.optimalTilingFeatures & need)) refuse("format %d lacks the %s image feature", (int)fmt, storage ? "storage" : "sampled");
    if ((storage == 1 || storage == 3) && !gfeatures.shaderStorageImageReadWithoutFormat)
        refuse("the device has no shaderStorageImageReadWithoutFormat, which a read storage image needs");
    if (storage >= 2 && !gfeatures.shaderStorageImageWriteWithoutFormat)
        refuse("the device has no shaderStorageImageWriteWithoutFormat, which a writable storage image needs");
    if (kind == TEX_CUBE_ARRAY && !gfeatures.imageCubeArray)
        refuse("the device has no imageCubeArray");
    VkImageFormatProperties ifp;
    if (vkGetPhysicalDeviceImageFormatProperties(gpd, fmt, texImageType(kind), VK_IMAGE_TILING_OPTIMAL,
                                                 texUsage(storage), texCreateFlags(kind), &ifp) != VK_SUCCESS)
        refuse("device lacks image support for %s texture of format %d", type, (int)fmt);
}

static void parseTextureLine(const char *line, unsigned maxTex, int allowWrite, unsigned *nt,
                             unsigned *tset, unsigned *tbind, unsigned *tw, unsigned *th, unsigned *td,
                             unsigned *tlayers, char (*tfile)[1024], char (*ttype)[16], char (*tout)[1024],
                             int *tstore, TexKind *tkind) {
    if (*nt >= maxTex) refuse("too many textures (max %u)", maxTex);
    unsigned i = *nt;
    strcpy(ttype[i], "float");
    if (tout) tout[i][0] = 0;
    td[i] = 1; tlayers[i] = 1; tkind[i] = TEX_2D;
    char extra[16] = "", dummy[1024] = "";
    char *out = tout ? tout[i] : dummy;
    int ok = 0;
    if (!strncmp(line, "texture ", 8)) {
        int n = sscanf(line, "texture %*u %u %u %u %u %1023s %15s %15s",
                       &tset[i], &tbind[i], &tw[i], &th[i], tfile[i], ttype[i], extra);
        if (n == 5 || n == 6) { tstore[i] = 0; ok = 1; }
    } else if (!strncmp(line, "storagetexture ", 15)) {
        char mode[16] = "";
        int n = sscanf(line, "storagetexture %*u %u %u %u %u %1023s %15s %15s %1023s %15s",
                       &tset[i], &tbind[i], &tw[i], &th[i], tfile[i], ttype[i], mode, out, extra);
        if (n == 5 || n == 6 || (allowWrite && n == 8)) {
            if (n == 8 && strcmp(mode, "write") && strcmp(mode, "readwrite"))
                die("storage texture mode %s is not write or readwrite", mode);
            tstore[i] = n < 8 ? 1 : mode[0] == 'w' ? 2 : 3;
            ok = 1;
        }
    } else {
        char kindStr[16] = "", mode[16] = "";
        int n = sscanf(line, "image %15s %15s %*u %u %u %u %u %u %u %15s %1023s %1023s %15s",
                       kindStr, mode, &tset[i], &tbind[i], &tw[i], &th[i], &td[i], &tlayers[i],
                       ttype[i], tfile[i], out, extra);
        if (n == 10 || (allowWrite && n == 11)) {
            tkind[i] = parseTexKind(kindStr);
            if (n == 10 && !strcmp(mode, "sample")) tstore[i] = 0;
            else if (n == 10 && !strcmp(mode, "read")) tstore[i] = 1;
            else if (n == 11 && !strcmp(mode, "write")) tstore[i] = 2;
            else if (n == 11 && !strcmp(mode, "readwrite")) tstore[i] = 3;
            else die("bad image mode %s in: %s", mode, line);
            ok = 1;
        }
    }
    if (!ok) die("bad manifest line: %s", line);
    if (!tw[i] || !th[i] || !td[i] || !tlayers[i]) die("zero texture dimension");
    (*nt)++;
}

static void writeFile(const char *path, const void *p, size_t n);
static int drawMain(const char *manifest);

// A graphics manifest starts with "mode draw"; a compute one has no mode line.
static int isDraw(const char *path) {
    char line[64] = "";
    FILE *f = fopen(path, "r");
    if (!f) die("cannot open %s", path);
    if (!fgets(line, sizeof line, f)) line[0] = 0;
    fclose(f);
    return strncmp(line, "mode draw", 9) == 0;
}

int main(int argc, char **argv) {
    if (argc != 2) die("usage: vkhost manifest");
    if (isDraw(argv[1])) return drawMain(argv[1]);
    char spv[1024] = "", entry[256] = "main", in[MAXBUF][1024], out[MAXBUF][1024];
    unsigned local[3] = {1, 1, 1}, groups[3] = {1, 1, 1}, set = 0, member[MAXBUF], nbuf = 0;
    unsigned long long bytes[MAXBUF];
    unsigned tset[MAXTEX], tbind[MAXTEX], tw[MAXTEX], th[MAXTEX], td[MAXTEX], tlayers[MAXTEX], tbpp[MAXTEX], nt = 0;
    TexKind tkind[MAXTEX];
    unsigned sset[MAXTEX], sbind[MAXTEX], sidx[MAXTEX], ns = 0, eaddr[MAXTEX][4];
    char tfile[MAXTEX][1024], ttype[MAXTEX][16], tout[MAXTEX][1024]; int tstore[MAXTEX];
    unsigned specid[MAXSPEC], specval[MAXSPEC], nspec = 0;
    unsigned bset[MAXTB], bbind[MAXTB], bcomp[MAXTB], btexels[MAXTB], bbytes[MAXTB], nb = 0; int bstore[MAXTB];
    char btype[MAXTB][16], bin[MAXTB][1024], bout[MAXTB][1024];
    FILE *mf = fopen(argv[1], "r");
    if (!mf) die("cannot open %s", argv[1]);
    char line[4096];
    while (fgets(line, sizeof line, mf)) {
        if (sscanf(line, "spv %1023s", spv) == 1 || sscanf(line, "entry %255s", entry) == 1) continue;
        if (sscanf(line, "set %u", &set) == 1) continue;
        if (nspec < MAXSPEC && sscanf(line, "spec %u %u", &specid[nspec], &specval[nspec]) == 2) { nspec++; continue; }
        if (sscanf(line, "local %u %u %u", local, local + 1, local + 2) == 3) continue;
        if (sscanf(line, "groups %u %u %u", groups, groups + 1, groups + 2) == 3) continue;
        if (nbuf < MAXBUF && sscanf(line, "buffer %u %llu %1023s %1023s", &member[nbuf], &bytes[nbuf], in[nbuf], out[nbuf]) == 4) { nbuf++; continue; }
        if (!strncmp(line, "texture ", 8) || !strncmp(line, "storagetexture ", 15) || !strncmp(line, "image ", 6)) {
            parseTextureLine(line, MAXTEX, 1, &nt, tset, tbind, tw, th, td, tlayers, tfile, ttype, tout, tstore, tkind);
            continue;
        }
        if (nb < MAXTB) {
            char kind[16];
            if (sscanf(line, "texelbuffer %*u %u %u %15s %15s %u %u %1023s %1023s", &bset[nb], &bbind[nb], kind, btype[nb], &bcomp[nb], &btexels[nb], bin[nb], bout[nb]) == 8) {
                if (strcmp(kind, "uniform") && strcmp(kind, "storage")) die("texel buffer kind %s is not uniform or storage", kind);
                bstore[nb++] = kind[0] == 's';
                continue;
            }
        }
        if (!strncmp(line, "sampler ", 8) || !strncmp(line, "esampler ", 9)) {
            if (ns >= MAXTEX) refuse("too many samplers (max %d)", MAXTEX);
            if (sscanf(line, "sampler %u %u %u", &sidx[ns], &sset[ns], &sbind[ns]) == 3) { eaddr[ns++][3] = 0; continue; }
            if (sscanf(line, "esampler %u %u %u %u %u %u", &sset[ns], &sbind[ns], &eaddr[ns][0], &eaddr[ns][1], &eaddr[ns][2], &sidx[ns]) == 6) { eaddr[ns++][3] = 1; continue; }
            die("bad manifest line: %s", line);
        }
        if (line[0] != '\n') die("bad manifest line: %s", line);
    }
    fclose(mf);
    if (!spv[0]) die("manifest has no spv");
    for (unsigned i = 0; i < nbuf; i++) if (member[i] != i) die("buffers must be listed as members 0..n-1 in order");

    uint32_t qf = initDevice(VK_QUEUE_COMPUTE_BIT);

    Buf bufs[MAXBUF]; VkDeviceAddress addr[MAXBUF + 1];
    for (unsigned i = 0; i < nbuf; i++) {
        bufs[i] = makeBuf(bytes[i], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 1);
        size_t n; void *data = slurp(in[i], &n);
        if (n != bytes[i]) die("%s is %zu bytes, manifest says %llu", in[i], n, bytes[i]);
        void *m; CHK(vkMapMemory(dev, bufs[i].mem, 0, VK_WHOLE_SIZE, 0, &m));
        memcpy(m, data, n); vkUnmapMemory(dev, bufs[i].mem); free(data);
        VkBufferDeviceAddressInfo ai = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        ai.buffer = bufs[i].buf;
        addr[i] = vkGetBufferDeviceAddress(dev, &ai);
        if (!addr[i]) die("device address of buffer %u is 0", i);
    }

    // Descriptor sets 0..maxset: the address block (binding 0 of `set`), then the textures and
    // samplers at the set and binding mslc's reflection gave. Sets in between stay empty.
    unsigned maxset = nbuf ? set : 0;
    for (unsigned i = 0; i < nt; i++) if (tset[i] > maxset) maxset = tset[i];
    for (unsigned i = 0; i < ns; i++) if (sset[i] > maxset) maxset = sset[i];
    for (unsigned i = 0; i < nb; i++) if (bset[i] > maxset) maxset = bset[i];
    if (maxset >= 8) die("descriptor set %u too large", maxset);
    unsigned nsets = nbuf || nt || ns || nb ? maxset + 1 : 0;
    VkDescriptorSetLayoutBinding lb[8][2 * MAXTEX + MAXTB + 1]; unsigned nlb[8] = {0};
    if (nbuf) lb[set][nlb[set]++] = (VkDescriptorSetLayoutBinding){0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL};
    for (unsigned i = 0; i < nt; i++) lb[tset[i]][nlb[tset[i]]++] = (VkDescriptorSetLayoutBinding){tbind[i], tstore[i] ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL};
    for (unsigned i = 0; i < ns; i++) lb[sset[i]][nlb[sset[i]]++] = (VkDescriptorSetLayoutBinding){sbind[i], VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL};
    for (unsigned i = 0; i < nb; i++) lb[bset[i]][nlb[bset[i]]++] = (VkDescriptorSetLayoutBinding){bbind[i], bstore[i] ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL};
    VkDescriptorSetLayout dsl[8]; VkDescriptorSet dset[8];
    VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    Img tex[MAXTEX]; Buf stage[MAXTEX], trb[MAXTEX]; Buf tbuf[MAXTB]; VkBufferView tview[MAXTB];
    for (unsigned i = 0; i < nt; i++) checkTextureSupport(tkind[i], ttype[i], tstore[i], &tbpp[i]);
    for (unsigned i = 0; i < nb; i++) {
        unsigned bytes1;
        VkFormat fmt = texelFormat(btype[i], bcomp[i], &bytes1);
        VkFormatProperties fp; vkGetPhysicalDeviceFormatProperties(gpd, fmt, &fp);
        VkFormatFeatureFlags need = bstore[i] ? VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT : VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT;
        if (!(fp.bufferFeatures & need)) refuse("format %d lacks the %s texel buffer feature", (int)fmt, bstore[i] ? "storage" : "uniform");
        if (bstore[i] && !(gfeatures.shaderStorageImageReadWithoutFormat && gfeatures.shaderStorageImageWriteWithoutFormat))
            refuse("the device has no shaderStorageImageRead/WriteWithoutFormat, which a storage texel buffer needs");
        VkPhysicalDeviceProperties dp; vkGetPhysicalDeviceProperties(gpd, &dp);
        if (btexels[i] > dp.limits.maxTexelBufferElements) refuse("%u texels exceed maxTexelBufferElements %u", btexels[i], dp.limits.maxTexelBufferElements);
        bbytes[i] = btexels[i] * bytes1;
        tbuf[i] = makeBuf(bbytes[i], bstore[i] ? VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT : VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, 0);
        size_t n; void *data = slurp(bin[i], &n);
        if (n != bbytes[i]) die("%s is %zu bytes, manifest says %u", bin[i], n, bbytes[i]);
        fillBuf(tbuf[i], data, n); free(data);
        VkBufferViewCreateInfo bvi = {VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO};
        bvi.buffer = tbuf[i].buf; bvi.format = fmt; bvi.range = VK_WHOLE_SIZE;
        CHK(vkCreateBufferView(dev, &bvi, NULL, &tview[i]));
    }
    if (nsets) {
        for (unsigned s = 0; s < nsets; s++) {
            VkDescriptorSetLayoutCreateInfo lci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            lci.bindingCount = nlb[s]; lci.pBindings = lb[s];
            CHK(vkCreateDescriptorSetLayout(dev, &lci, NULL, &dsl[s]));
        }
        plci.setLayoutCount = nsets; plci.pSetLayouts = dsl;
        VkDescriptorPoolSize ps[6] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAXTEX}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAXTEX}, {VK_DESCRIPTOR_TYPE_SAMPLER, MAXTEX},
                                      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, MAXTB}, {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, MAXTB}};
        VkDescriptorPoolCreateInfo pci = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pci.maxSets = nsets; pci.poolSizeCount = 6; pci.pPoolSizes = ps;
        VkDescriptorPool pool; CHK(vkCreateDescriptorPool(dev, &pci, NULL, &pool));
        VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dai.descriptorPool = pool; dai.descriptorSetCount = nsets; dai.pSetLayouts = dsl;
        CHK(vkAllocateDescriptorSets(dev, &dai, dset));
        VkWriteDescriptorSet wr[1 + 2 * MAXTEX + MAXTB]; unsigned nw = 0;
        VkDescriptorBufferInfo dbi; VkDescriptorImageInfo ii[MAXTEX], si[MAXTEX];
        if (nbuf) {
            Buf blk = makeBuf(8 * nbuf, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 0);
            fillBuf(blk, addr, 8 * nbuf);
            dbi = (VkDescriptorBufferInfo){blk.buf, 0, blk.size};
            wr[nw] = (VkWriteDescriptorSet){VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wr[nw].dstSet = dset[set]; wr[nw].descriptorCount = 1; wr[nw].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            wr[nw++].pBufferInfo = &dbi;
        }
        for (unsigned i = 0; i < nt; i++) {
            tex[i] = loadTexture(tkind[i], tfile[i], ttype[i], tw[i], th[i], td[i], tlayers[i], tstore[i], &stage[i], &tbpp[i]);
            if (tstore[i] >= 2) trb[i] = makeBuf((size_t)tw[i] * th[i] * td[i] * texSlices(tkind[i], tlayers[i]) * tbpp[i],
                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0);
            ii[i] = (VkDescriptorImageInfo){VK_NULL_HANDLE, tex[i].view, tstore[i] ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            wr[nw] = (VkWriteDescriptorSet){VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wr[nw].dstSet = dset[tset[i]]; wr[nw].dstBinding = tbind[i]; wr[nw].descriptorCount = 1;
            wr[nw].descriptorType = tstore[i] ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE; wr[nw++].pImageInfo = &ii[i];
        }
        for (unsigned i = 0; i < ns; i++) {
            VkSampler sampler = eaddr[i][3] ? makeSampler(eaddr[i][0], eaddr[i][1], eaddr[i][2], sidx[i] ? VK_FILTER_LINEAR : VK_FILTER_NEAREST)
                                            : makeNearestSampler(sidx[i]);
            si[i] = (VkDescriptorImageInfo){sampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
            wr[nw] = (VkWriteDescriptorSet){VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wr[nw].dstSet = dset[sset[i]]; wr[nw].dstBinding = sbind[i]; wr[nw].descriptorCount = 1;
            wr[nw].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER; wr[nw++].pImageInfo = &si[i];
        }
        for (unsigned i = 0; i < nb; i++) {
            wr[nw] = (VkWriteDescriptorSet){VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wr[nw].dstSet = dset[bset[i]]; wr[nw].dstBinding = bbind[i]; wr[nw].descriptorCount = 1;
            wr[nw].descriptorType = bstore[i] ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER; wr[nw++].pTexelBufferView = &tview[i];
        }
        if (nw) vkUpdateDescriptorSets(dev, nw, wr, 0, NULL);
    }
    VkPipelineLayout layout;
    CHK(vkCreatePipelineLayout(dev, &plci, NULL, &layout));

    size_t codeSize; uint32_t *code = slurp(spv, &codeSize);
    VkShaderModuleCreateInfo smci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = codeSize; smci.pCode = code;
    VkShaderModule mod; CHK(vkCreateShaderModule(dev, &smci, NULL, &mod));
    VkSpecializationMapEntry me[3 + MAXSPEC] = {{0, 0, 4}, {1, 4, 4}, {2, 8, 4}};
    unsigned sdata[3 + MAXSPEC] = {local[0], local[1], local[2]};
    for (unsigned i = 0; i < nspec; i++) { me[3 + i] = (VkSpecializationMapEntry){specid[i], (3 + i) * 4, 4}; sdata[3 + i] = specval[i]; }
    VkSpecializationInfo si = {3 + nspec, me, (3 + nspec) * 4, sdata};
    VkComputePipelineCreateInfo pci = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; pci.stage.module = mod;
    pci.stage.pName = entry; pci.stage.pSpecializationInfo = &si; pci.layout = layout;
    VkPipeline pipe; CHK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &pci, NULL, &pipe));

    VkCommandPoolCreateInfo cpi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpi.queueFamilyIndex = qf;
    VkCommandPool cp; CHK(vkCreateCommandPool(dev, &cpi, NULL, &cp));
    VkCommandBufferAllocateInfo cai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = cp; cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount = 1;
    VkCommandBuffer cmd; CHK(vkAllocateCommandBuffers(dev, &cai, &cmd));
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    CHK(vkBeginCommandBuffer(cmd, &bi));
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
    for (unsigned i = 0; i < nt; i++) recordTextureUpload(cmd, tex[i], stage[i], tkind[i], tw[i], th[i], td[i], tlayers[i], tstore[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    if (nsets) vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, nsets, dset, 0, NULL);
    vkCmdDispatch(cmd, groups[0], groups[1], groups[2]);
    for (unsigned i = 0; i < nt; i++) if (tstore[i] >= 2) {
        unsigned totalLayers = texSlices(tkind[i], tlayers[i]);
        VkImageMemoryBarrier post = imgBarrier(tex[i].img, VK_IMAGE_ASPECT_COLOR_BIT, totalLayers, VK_IMAGE_LAYOUT_GENERAL,
                                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &post);
        VkBufferImageCopy bc = {0, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, totalLayers}, {0, 0, 0}, {tw[i], th[i], td[i]}};
        vkCmdCopyImageToBuffer(cmd, tex[i].img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, trb[i].buf, 1, &bc);
    }
    VkMemoryBarrier mb = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT; mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0, NULL);
    CHK(vkEndCommandBuffer(cmd));
    VkQueue q; vkGetDeviceQueue(dev, qf, 0, &q);
    VkSubmitInfo sub = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    sub.commandBufferCount = 1; sub.pCommandBuffers = &cmd;
    CHK(vkQueueSubmit(q, 1, &sub, VK_NULL_HANDLE));
    CHK(vkQueueWaitIdle(q));

    for (unsigned i = 0; i < nbuf; i++) {
        void *m; CHK(vkMapMemory(dev, bufs[i].mem, 0, VK_WHOLE_SIZE, 0, &m));
        FILE *f = fopen(out[i], "wb");
        if (!f || fwrite(m, 1, bytes[i], f) != bytes[i]) die("cannot write %s", out[i]);
        fclose(f); vkUnmapMemory(dev, bufs[i].mem);
    }
    for (unsigned i = 0; i < nt; i++) if (tstore[i] >= 2) {
        void *m; CHK(vkMapMemory(dev, trb[i].mem, 0, VK_WHOLE_SIZE, 0, &m));
        writeFile(tout[i], m, (size_t)tw[i] * th[i] * td[i] * texSlices(tkind[i], tlayers[i]) * tbpp[i]);
        vkUnmapMemory(dev, trb[i].mem);
    }
    for (unsigned i = 0; i < nb; i++) {
        void *m; CHK(vkMapMemory(dev, tbuf[i].mem, 0, VK_WHOLE_SIZE, 0, &m));
        writeFile(bout[i], m, bbytes[i]); vkUnmapMemory(dev, tbuf[i].mem);
    }
    return 0;
}

// ---- graphics: one fixed draw into offscreen targets (see tools/diffcheck/graphics.py) ----
//
// Manifest lines: mode draw, spv vertex|fragment FILE, vertex NAME, fragment NAME, target W H,
// draw V I, attr LOC FMT FILE, color I FMT C0 C1 C2 C3 OUT, depth CLEAR OUT,
// gbuffer STAGE INDEX SET MEMBER BYTES FILE [OUTFILE], texture|storagetexture INDEX SET BINDING W H FILE [TYPE],
// image KIND ACCESS INDEX SET BINDING W H D LAYERS TYPE FILE,
// sampler INDEX SET BINDING, esampler SET BINDING S T R FILTER. Same bytes and same names as metalhost's draw mode.
// The viewport has a negative height so framebuffer row 0 is the top, as in Metal.
// Read-back: colour/depth attachment write -> transfer read (image barrier), then transfer
// write -> host read, the barrier audited in PR #148.
#define MAXATTR 16
#define MAXCOLOR 8
#define MAXG 16
#define MAXDTEX 8

typedef struct { const char *name; VkFormat format; unsigned size; } Fmt;
static const Fmt VFMT[] = {
    {"float", VK_FORMAT_R32_SFLOAT, 4}, {"float2", VK_FORMAT_R32G32_SFLOAT, 8}, {"float3", VK_FORMAT_R32G32B32_SFLOAT, 12},
    {"float4", VK_FORMAT_R32G32B32A32_SFLOAT, 16}, {"int", VK_FORMAT_R32_SINT, 4}, {"int2", VK_FORMAT_R32G32_SINT, 8},
    {"int3", VK_FORMAT_R32G32B32_SINT, 12}, {"int4", VK_FORMAT_R32G32B32A32_SINT, 16}, {"uint", VK_FORMAT_R32_UINT, 4},
    {"uint2", VK_FORMAT_R32G32_UINT, 8}, {"uint3", VK_FORMAT_R32G32B32_UINT, 12}, {"uint4", VK_FORMAT_R32G32B32A32_UINT, 16},
    {"half", VK_FORMAT_R16_SFLOAT, 2}, {"half2", VK_FORMAT_R16G16_SFLOAT, 4}, {"half3", VK_FORMAT_R16G16B16_SFLOAT, 6},
    {"half4", VK_FORMAT_R16G16B16A16_SFLOAT, 8},
};
static const Fmt CFMT[] = {
    {"rgba32float", VK_FORMAT_R32G32B32A32_SFLOAT, 16}, {"rgba32sint", VK_FORMAT_R32G32B32A32_SINT, 16},
    {"rgba32uint", VK_FORMAT_R32G32B32A32_UINT, 16}, {"rgba16float", VK_FORMAT_R16G16B16A16_SFLOAT, 8},
    {"rg32float", VK_FORMAT_R32G32_SFLOAT, 8}, {"rg32sint", VK_FORMAT_R32G32_SINT, 8},
    {"rg32uint", VK_FORMAT_R32G32_UINT, 8}, {"rg16float", VK_FORMAT_R16G16_SFLOAT, 4},
    {"r32float", VK_FORMAT_R32_SFLOAT, 4}, {"r32sint", VK_FORMAT_R32_SINT, 4},
    {"r32uint", VK_FORMAT_R32_UINT, 4}, {"r16float", VK_FORMAT_R16_SFLOAT, 2},
};

static const Fmt *findFmt(const Fmt *t, size_t n, const char *name) {
    for (size_t i = 0; i < n; i++) if (!strcmp(t[i].name, name)) return &t[i];
    die("unknown format %s", name);
}

static VkFormat texelFormat(const char *type, unsigned components, unsigned *bytes) {
    static const struct { const char *type; unsigned components; VkFormat format; unsigned bytes; } T[] = {
        {"float", 1, VK_FORMAT_R32_SFLOAT, 4}, {"float", 2, VK_FORMAT_R32G32_SFLOAT, 8}, {"float", 4, VK_FORMAT_R32G32B32A32_SFLOAT, 16},
        {"half", 1, VK_FORMAT_R16_SFLOAT, 2}, {"half", 2, VK_FORMAT_R16G16_SFLOAT, 4}, {"half", 4, VK_FORMAT_R16G16B16A16_SFLOAT, 8},
        {"int", 1, VK_FORMAT_R32_SINT, 4}, {"int", 2, VK_FORMAT_R32G32_SINT, 8}, {"int", 4, VK_FORMAT_R32G32B32A32_SINT, 16},
        {"uint", 1, VK_FORMAT_R32_UINT, 4}, {"uint", 2, VK_FORMAT_R32G32_UINT, 8}, {"uint", 4, VK_FORMAT_R32G32B32A32_UINT, 16},
        {"short", 1, VK_FORMAT_R16_SINT, 2}, {"short", 2, VK_FORMAT_R16G16_SINT, 4}, {"short", 4, VK_FORMAT_R16G16B16A16_SINT, 8},
        {"ushort", 1, VK_FORMAT_R16_UINT, 2}, {"ushort", 2, VK_FORMAT_R16G16_UINT, 4}, {"ushort", 4, VK_FORMAT_R16G16B16A16_UINT, 8},
    };
    for (size_t i = 0; i < sizeof T / sizeof *T; i++)
        if (!strcmp(T[i].type, type) && T[i].components == components) { *bytes = T[i].bytes; return T[i].format; }
    die("unknown texel buffer type %s with %u components", type, components);
}

static uint32_t memTypeAny(uint32_t bits) {
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++) if (bits & (1u << i)) return i;
    die("no memory type");
}

static TexKind parseTexKind(const char *s) {
    if (!strcmp(s, "1d")) return TEX_1D;
    if (!strcmp(s, "1d_array")) return TEX_1D_ARRAY;
    if (!strcmp(s, "2d")) return TEX_2D;
    if (!strcmp(s, "2d_array")) return TEX_2D_ARRAY;
    if (!strcmp(s, "3d")) return TEX_3D;
    if (!strcmp(s, "cube")) return TEX_CUBE;
    if (!strcmp(s, "cube_array")) return TEX_CUBE_ARRAY;
    die("unknown texture kind %s", s);
}

static unsigned texSlices(TexKind kind, unsigned layers) {
    return (kind == TEX_CUBE || kind == TEX_CUBE_ARRAY) ? 6 * layers : layers;
}

static VkImageType texImageType(TexKind kind) {
    return (kind == TEX_1D || kind == TEX_1D_ARRAY) ? VK_IMAGE_TYPE_1D
        : (kind == TEX_3D) ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
}

static VkImageCreateFlags texCreateFlags(TexKind kind) {
    return (kind == TEX_CUBE || kind == TEX_CUBE_ARRAY) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
}

static VkImageUsageFlags texUsage(int storage) {
    return (storage ? VK_IMAGE_USAGE_STORAGE_BIT : VK_IMAGE_USAGE_SAMPLED_BIT) | VK_IMAGE_USAGE_TRANSFER_DST_BIT
        | (storage >= 2 ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0);
}

static Img makeImage(TexKind kind, VkFormat f, unsigned w, unsigned h, unsigned d, unsigned layers,
                     VkImageUsageFlags usage, VkImageAspectFlags aspect) {
    Img r = {0};
    unsigned totalLayers = texSlices(kind, layers);
    VkImageCreateInfo ci = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.flags = texCreateFlags(kind);
    ci.imageType = texImageType(kind);
    ci.format = f; ci.extent = (VkExtent3D){w, h, d}; ci.mipLevels = 1; ci.arrayLayers = totalLayers;
    ci.samples = VK_SAMPLE_COUNT_1_BIT; ci.tiling = VK_IMAGE_TILING_OPTIMAL; ci.usage = usage;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    CHK(vkCreateImage(dev, &ci, NULL, &r.img));
    VkMemoryRequirements req; vkGetImageMemoryRequirements(dev, r.img, &req);
    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size; ai.memoryTypeIndex = memTypeAny(req.memoryTypeBits);
    CHK(vkAllocateMemory(dev, &ai, NULL, &r.mem));
    CHK(vkBindImageMemory(dev, r.img, r.mem, 0));
    VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = r.img; vi.format = f;
    vi.viewType = kind == TEX_1D ? VK_IMAGE_VIEW_TYPE_1D : kind == TEX_1D_ARRAY ? VK_IMAGE_VIEW_TYPE_1D_ARRAY
        : kind == TEX_2D ? VK_IMAGE_VIEW_TYPE_2D : kind == TEX_2D_ARRAY ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
        : kind == TEX_3D ? VK_IMAGE_VIEW_TYPE_3D : kind == TEX_CUBE ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    vi.subresourceRange = (VkImageSubresourceRange){aspect, 0, 1, 0, totalLayers};
    CHK(vkCreateImageView(dev, &vi, NULL, &r.view));
    return r;
}

static void fillBuf(Buf b, const void *data, size_t n) {
    void *m; CHK(vkMapMemory(dev, b.mem, 0, VK_WHOLE_SIZE, 0, &m));
    memcpy(m, data, n); vkUnmapMemory(dev, b.mem);
}

static VkImageMemoryBarrier imgBarrier(VkImage img, VkImageAspectFlags aspect, unsigned layers,
                                       VkImageLayout from, VkImageLayout to, VkAccessFlags src, VkAccessFlags dst) {
    VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcAccessMask = src; b.dstAccessMask = dst; b.oldLayout = from; b.newLayout = to;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; b.image = img;
    b.subresourceRange = (VkImageSubresourceRange){aspect, 0, 1, 0, layers};
    return b;
}

static void writeFile(const char *path, const void *p, size_t n) {
    FILE *f = fopen(path, "wb");
    if (!f || fwrite(p, 1, n, f) != n) die("cannot write %s", path);
    fclose(f);
}

static int drawMain(const char *manifest) {
    char spv[2][1024] = {"", ""}, vname[256] = "", fname[256] = "";
    unsigned width = 0, height = 0, nverts = 0, ninst = 1;
    unsigned na = 0, nc = 0, ng = 0, nt = 0, ns = 0, hasDepth = 0;
    unsigned aloc[MAXATTR]; char afmt[MAXATTR][32], afile[MAXATTR][1024];
    unsigned cidx[MAXCOLOR]; char cfmt[MAXCOLOR][32], cout[MAXCOLOR][1024]; double cclear[MAXCOLOR][4];
    double dclear = 1; char dout[1024] = "";
    char gstage[MAXG][16], gfile[MAXG][1024], gout[MAXG][1024]; unsigned gidx[MAXG], gset[MAXG], gmember[MAXG]; unsigned long long gbytes[MAXG];
    unsigned tset[MAXDTEX], tbind[MAXDTEX], tw[MAXDTEX], th[MAXDTEX], td[MAXDTEX], tlayers[MAXDTEX], tbpp[MAXDTEX];
    TexKind tkind[MAXDTEX];
    char tfile[MAXDTEX][1024], ttype[MAXDTEX][16]; int tstore[MAXDTEX];
    unsigned sset[MAXDTEX], sbind[MAXDTEX], sidx[MAXDTEX], eaddr[MAXDTEX][4];
    FILE *mf = fopen(manifest, "r");
    if (!mf) die("cannot open %s", manifest);
    char line[4096], word[32];
    while (fgets(line, sizeof line, mf)) {
        if (line[0] == '\n' || !strncmp(line, "mode ", 5)) continue;
        if (sscanf(line, "spv %31s", word) == 1) {
            int which = !strcmp(word, "fragment");
            if (sscanf(line, "spv %*s %1023s", spv[which]) != 1) die("bad manifest line: %s", line);
        } else if (sscanf(line, "vertex %255s", vname) == 1 || sscanf(line, "fragment %255s", fname) == 1) {
        } else if (sscanf(line, "target %u %u", &width, &height) == 2) {
        } else if (sscanf(line, "draw %u %u", &nverts, &ninst) == 2) {
        } else if (na < MAXATTR && sscanf(line, "attr %u %31s %1023s", &aloc[na], afmt[na], afile[na]) == 3) { na++;
        } else if (nc < MAXCOLOR && sscanf(line, "color %u %31s %lf %lf %lf %lf %1023s", &cidx[nc], cfmt[nc], &cclear[nc][0],
                       &cclear[nc][1], &cclear[nc][2], &cclear[nc][3], cout[nc]) == 7) { nc++;
        } else if (sscanf(line, "depth %lf %1023s", &dclear, dout) == 2) { hasDepth = 1;
        } else if (!strncmp(line, "gbuffer ", 8)) {
            if (ng >= MAXG) refuse("too many buffers");
            gout[ng][0] = 0;
            int n = sscanf(line, "gbuffer %15s %u %u %u %llu %1023s %1023s", gstage[ng], &gidx[ng], &gset[ng], &gmember[ng],
                           &gbytes[ng], gfile[ng], gout[ng]);
            if ((n != 6 && n != 7) || (strcmp(gstage[ng], "vertex") && strcmp(gstage[ng], "fragment")))
                die("bad manifest line: %s", line);
            ng++;
        } else if (!strncmp(line, "texture ", 8) || !strncmp(line, "storagetexture ", 15) || !strncmp(line, "image ", 6)) {
            parseTextureLine(line, MAXDTEX, 0, &nt, tset, tbind, tw, th, td, tlayers, tfile, ttype, NULL, tstore, tkind);
        } else if (!strncmp(line, "sampler ", 8)) {
            if (ns >= MAXDTEX) refuse("too many samplers");
            char extra[16] = "";
            if (sscanf(line, "sampler %u %u %u %15s", &sidx[ns], &sset[ns], &sbind[ns], extra) != 3) die("bad manifest line: %s", line);
            eaddr[ns++][3] = 0;
        } else if (!strncmp(line, "esampler ", 9)) {
            if (ns >= MAXDTEX) refuse("too many samplers");
            char extra[16] = "";
            if (sscanf(line, "esampler %u %u %u %u %u %u %15s", &sset[ns], &sbind[ns], &eaddr[ns][0], &eaddr[ns][1], &eaddr[ns][2], &sidx[ns], extra) != 6) die("bad manifest line: %s", line);
            eaddr[ns++][3] = 1;
        } else die("bad manifest line: %s", line);
    }
    fclose(mf);
    if (!spv[0][0] || !spv[1][0] || !vname[0] || !fname[0] || !width || !height || !nverts) die("incomplete draw manifest");
    for (unsigned i = 0; i < nc; i++) if (cidx[i] != i) die("colour targets must be listed 0..n-1 in order");

    uint32_t qf = initDevice(VK_QUEUE_GRAPHICS_BIT);
    for (unsigned i = 0; i < nt; i++) checkTextureSupport(tkind[i], ttype[i], tstore[i], &tbpp[i]);

    // Address blocks, one uniform buffer per stage that has buffer arguments.
    Buf blk[2] = {{0}, {0}}; unsigned blkSet[2] = {0, 0}, nmember[2] = {0, 0}, haveBlk[2] = {0, 0};
    VkDeviceAddress addr[2][MAXG]; Buf gb[MAXG];
    for (unsigned i = 0; i < ng; i++) {
        int st = !strcmp(gstage[i], "fragment");
        gb[i] = makeBuf(gbytes[i], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 1);
        size_t n; void *data = slurp(gfile[i], &n);
        if (n != gbytes[i]) die("%s is %zu bytes, manifest says %llu", gfile[i], n, gbytes[i]);
        fillBuf(gb[i], data, n); free(data);
        VkBufferDeviceAddressInfo ai = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO}; ai.buffer = gb[i].buf;
        if (gmember[i] >= MAXG) die("member %u too large", gmember[i]);
        addr[st][gmember[i]] = vkGetBufferDeviceAddress(dev, &ai);
        if (haveBlk[st] && blkSet[st] != gset[i]) die("one stage's buffers must share a set");
        haveBlk[st] = 1; blkSet[st] = gset[i]; if (gmember[i] + 1 > nmember[st]) nmember[st] = gmember[i] + 1;
    }
    if (haveBlk[0] && haveBlk[1] && blkSet[0] == blkSet[1]) die("both stages use the address block of set %u", blkSet[0]);
    for (int st = 0; st < 2; st++) if (haveBlk[st]) {
        blk[st] = makeBuf(8 * nmember[st], VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 0);
        fillBuf(blk[st], addr[st], 8 * nmember[st]);
    }

    // Descriptor sets 0..maxset; empty sets are still created so set numbers line up.
    unsigned maxset = 0;
    for (int st = 0; st < 2; st++) if (haveBlk[st] && blkSet[st] > maxset) maxset = blkSet[st];
    for (unsigned i = 0; i < nt; i++) if (tset[i] > maxset) maxset = tset[i];
    for (unsigned i = 0; i < ns; i++) if (sset[i] > maxset) maxset = sset[i];
    if (maxset >= 8) die("descriptor set %u too large", maxset);
    VkDescriptorSetLayoutBinding lb[8][1 + 2 * MAXDTEX]; unsigned nlb[8] = {0};
    for (int st = 0; st < 2; st++) if (haveBlk[st]) {
        unsigned s = blkSet[st];
        lb[s][nlb[s]++] = (VkDescriptorSetLayoutBinding){0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
            st ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT, NULL};
    }
    for (unsigned i = 0; i < nt; i++) {
        if (nlb[tset[i]] >= 1 + 2 * MAXDTEX) die("too many bindings in set %u", tset[i]);
        lb[tset[i]][nlb[tset[i]]++] = (VkDescriptorSetLayoutBinding){tbind[i],
            tstore[i] ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, NULL};
    }
    for (unsigned i = 0; i < ns; i++) {
        if (nlb[sset[i]] >= 1 + 2 * MAXDTEX) die("too many bindings in set %u", sset[i]);
        lb[sset[i]][nlb[sset[i]]++] = (VkDescriptorSetLayoutBinding){sbind[i],
            VK_DESCRIPTOR_TYPE_SAMPLER, 1,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, NULL};
    }
    VkDescriptorSetLayout dsl[8]; VkDescriptorSet dset[8];
    for (unsigned s = 0; s <= maxset; s++) {
        VkDescriptorSetLayoutCreateInfo lci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        lci.bindingCount = nlb[s]; lci.pBindings = lb[s];
        CHK(vkCreateDescriptorSetLayout(dev, &lci, NULL, &dsl[s]));
    }
    VkDescriptorPoolSize ps[4] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAXDTEX},
                                  {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAXDTEX}, {VK_DESCRIPTOR_TYPE_SAMPLER, MAXDTEX}};
    VkDescriptorPoolCreateInfo dpi = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpi.maxSets = maxset + 1; dpi.poolSizeCount = 4; dpi.pPoolSizes = ps;
    VkDescriptorPool pool; CHK(vkCreateDescriptorPool(dev, &dpi, NULL, &pool));
    VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool = pool; dai.descriptorSetCount = maxset + 1; dai.pSetLayouts = dsl;
    CHK(vkAllocateDescriptorSets(dev, &dai, dset));
    VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = maxset + 1; plci.pSetLayouts = dsl;
    VkPipelineLayout layout; CHK(vkCreatePipelineLayout(dev, &plci, NULL, &layout));

    VkWriteDescriptorSet wr[2 + 2 * MAXDTEX]; unsigned nw = 0;
    VkDescriptorBufferInfo bi[2]; VkDescriptorImageInfo ii[MAXDTEX], si[MAXDTEX];
    for (int st = 0; st < 2; st++) if (haveBlk[st]) {
        bi[st] = (VkDescriptorBufferInfo){blk[st].buf, 0, 8 * nmember[st]};
        wr[nw] = (VkWriteDescriptorSet){VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        wr[nw].dstSet = dset[blkSet[st]]; wr[nw].dstBinding = 0; wr[nw].descriptorCount = 1;
        wr[nw].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; wr[nw++].pBufferInfo = &bi[st];
    }
    Img tex[MAXDTEX]; Buf stage[MAXDTEX];
    for (unsigned i = 0; i < nt; i++) {
        unsigned bpp;
        tex[i] = loadTexture(tkind[i], tfile[i], ttype[i], tw[i], th[i], td[i], tlayers[i], tstore[i], &stage[i], &bpp);
        ii[i] = (VkDescriptorImageInfo){VK_NULL_HANDLE, tex[i].view,
            tstore[i] ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        wr[nw] = (VkWriteDescriptorSet){VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        wr[nw].dstSet = dset[tset[i]]; wr[nw].dstBinding = tbind[i]; wr[nw].descriptorCount = 1;
        wr[nw].descriptorType = tstore[i] ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        wr[nw++].pImageInfo = &ii[i];
    }
    for (unsigned i = 0; i < ns; i++) {
        VkSampler sampler = eaddr[i][3] ? makeSampler(eaddr[i][0], eaddr[i][1], eaddr[i][2], sidx[i] ? VK_FILTER_LINEAR : VK_FILTER_NEAREST)
                                        : makeNearestSampler(sidx[i]);
        si[i] = (VkDescriptorImageInfo){sampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
        wr[nw] = (VkWriteDescriptorSet){VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        wr[nw].dstSet = dset[sset[i]]; wr[nw].dstBinding = sbind[i]; wr[nw].descriptorCount = 1;
        wr[nw].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER; wr[nw++].pImageInfo = &si[i];
    }
    if (nw) vkUpdateDescriptorSets(dev, nw, wr, 0, NULL);

    // Vertex buffers and the vertex input state: binding i carries attribute i, tightly packed.
    Buf vb[MAXATTR]; VkVertexInputBindingDescription vbd[MAXATTR]; VkVertexInputAttributeDescription vad[MAXATTR];
    for (unsigned i = 0; i < na; i++) {
        const Fmt *f = findFmt(VFMT, sizeof VFMT / sizeof *VFMT, afmt[i]);
        size_t n; void *data = slurp(afile[i], &n);
        if (n != (size_t)f->size * nverts) die("%s is %zu bytes, %u vertices of %s need %zu", afile[i], n, nverts, afmt[i], (size_t)f->size * nverts);
        vb[i] = makeBuf(n, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 0); fillBuf(vb[i], data, n); free(data);
        vbd[i] = (VkVertexInputBindingDescription){i, f->size, VK_VERTEX_INPUT_RATE_VERTEX};
        vad[i] = (VkVertexInputAttributeDescription){aloc[i], i, f->format, 0};
    }

    Img color[MAXCOLOR]; Buf rb[MAXCOLOR]; VkFormat cf[MAXCOLOR]; unsigned cbytes[MAXCOLOR];
    for (unsigned i = 0; i < nc; i++) {
        const Fmt *f = findFmt(CFMT, sizeof CFMT / sizeof *CFMT, cfmt[i]);
        cf[i] = f->format; cbytes[i] = f->size;
        color[i] = makeImage(TEX_2D, f->format, width, height, 1, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        rb[i] = makeBuf((size_t)width * height * f->size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0);
    }
    Img depth = {0}; Buf drb = {0};
    if (hasDepth) {
        depth = makeImage(TEX_2D, VK_FORMAT_D32_SFLOAT, width, height, 1, 1, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
        drb = makeBuf((size_t)width * height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0);
    }

    VkShaderModule mod[2];
    for (int i = 0; i < 2; i++) {
        size_t n; uint32_t *code = slurp(spv[i], &n);
        VkShaderModuleCreateInfo smci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; smci.codeSize = n; smci.pCode = code;
        CHK(vkCreateShaderModule(dev, &smci, NULL, &mod[i]));
    }
    VkPipelineShaderStageCreateInfo stages[2] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_VERTEX_BIT, mod[0], vname, NULL},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT, mod[1], fname, NULL}};
    VkPipelineVertexInputStateCreateInfo vis = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vis.vertexBindingDescriptionCount = na; vis.pVertexBindingDescriptions = vbd;
    vis.vertexAttributeDescriptionCount = na; vis.pVertexAttributeDescriptions = vad;
    VkPipelineInputAssemblyStateCreateInfo ias = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ias.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport vp = {0, (float)height, (float)width, -(float)height, 0, 1};
    VkRect2D sc = {{0, 0}, {width, height}};
    VkPipelineViewportStateCreateInfo vps = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vps.viewportCount = 1; vps.pViewports = &vp; vps.scissorCount = 1; vps.pScissors = &sc;
    VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE; rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth = 1;
    VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo dss = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    dss.depthTestEnable = hasDepth; dss.depthWriteEnable = hasDepth; dss.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    VkPipelineColorBlendAttachmentState cba[MAXCOLOR];
    for (unsigned i = 0; i < nc; i++) cba[i] = (VkPipelineColorBlendAttachmentState){.colorWriteMask = 0xF};
    VkPipelineColorBlendStateCreateInfo cbs = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cbs.attachmentCount = nc; cbs.pAttachments = cba;
    VkPipelineRenderingCreateInfo ri = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    ri.colorAttachmentCount = nc; ri.pColorAttachmentFormats = cf; ri.depthAttachmentFormat = hasDepth ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_UNDEFINED;
    VkGraphicsPipelineCreateInfo gpi = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gpi.pNext = &ri; gpi.stageCount = 2; gpi.pStages = stages; gpi.pVertexInputState = &vis; gpi.pInputAssemblyState = &ias;
    gpi.pViewportState = &vps; gpi.pRasterizationState = &rs; gpi.pMultisampleState = &ms; gpi.pDepthStencilState = &dss;
    gpi.pColorBlendState = &cbs; gpi.layout = layout;
    VkPipeline pipe; CHK(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpi, NULL, &pipe));

    VkCommandPoolCreateInfo cpi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; cpi.queueFamilyIndex = qf;
    VkCommandPool cp; CHK(vkCreateCommandPool(dev, &cpi, NULL, &cp));
    VkCommandBufferAllocateInfo cai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = cp; cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount = 1;
    VkCommandBuffer cmd; CHK(vkAllocateCommandBuffers(dev, &cai, &cmd));
    VkCommandBufferBeginInfo cbi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; CHK(vkBeginCommandBuffer(cmd, &cbi));

    VkImageMemoryBarrier pre[MAXCOLOR + 1]; unsigned npre = 0;
    for (unsigned i = 0; i < nc; i++) pre[npre++] = imgBarrier(color[i].img, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    if (hasDepth) pre[npre++] = imgBarrier(depth.img, VK_IMAGE_ASPECT_DEPTH_BIT, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
    if (npre) vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, npre, pre);
    for (unsigned i = 0; i < nt; i++)
        recordTextureUpload(cmd, tex[i], stage[i], tkind[i], tw[i], th[i], td[i], tlayers[i], tstore[i],
                            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    VkRenderingAttachmentInfo ca[MAXCOLOR], da = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    for (unsigned i = 0; i < nc; i++) {
        ca[i] = (VkRenderingAttachmentInfo){VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        ca[i].imageView = color[i].view; ca[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        ca[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; ca[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        if (strstr(cfmt[i], "sint")) for (int k = 0; k < 4; k++) ca[i].clearValue.color.int32[k] = (int32_t)cclear[i][k];
        else if (strstr(cfmt[i], "uint")) for (int k = 0; k < 4; k++) ca[i].clearValue.color.uint32[k] = (uint32_t)cclear[i][k];
        else for (int k = 0; k < 4; k++) ca[i].clearValue.color.float32[k] = (float)cclear[i][k];
    }
    if (hasDepth) {
        da.imageView = depth.view; da.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        da.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; da.storeOp = VK_ATTACHMENT_STORE_OP_STORE; da.clearValue.depthStencil.depth = (float)dclear;
    }
    VkRenderingInfo rinfo = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    rinfo.renderArea = sc; rinfo.layerCount = 1; rinfo.colorAttachmentCount = nc; rinfo.pColorAttachments = ca;
    rinfo.pDepthAttachment = hasDepth ? &da : NULL;
    vkCmdBeginRendering(cmd, &rinfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, maxset + 1, dset, 0, NULL);
    for (unsigned i = 0; i < na; i++) { VkDeviceSize off = 0; vkCmdBindVertexBuffers(cmd, i, 1, &vb[i].buf, &off); }
    vkCmdDraw(cmd, nverts, ninst, 0, 0);
    vkCmdEndRendering(cmd);

    VkImageMemoryBarrier post[MAXCOLOR + 1]; unsigned npost = 0;
    for (unsigned i = 0; i < nc; i++) post[npost++] = imgBarrier(color[i].img, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    if (hasDepth) post[npost++] = imgBarrier(depth.img, VK_IMAGE_ASPECT_DEPTH_BIT, 1, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    if (npost) vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, npost, post);
    for (unsigned i = 0; i < nc; i++) {
        VkBufferImageCopy bc = {0, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {width, height, 1}};
        vkCmdCopyImageToBuffer(cmd, color[i].img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, rb[i].buf, 1, &bc);
    }
    if (hasDepth) {
        VkBufferImageCopy bc = {0, 0, 0, {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1}, {0, 0, 0}, {width, height, 1}};
        vkCmdCopyImageToBuffer(cmd, depth.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, drb.buf, 1, &bc);
    }
    VkMemoryBarrier mb = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT; mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0, NULL);
    CHK(vkEndCommandBuffer(cmd));
    VkQueue q; vkGetDeviceQueue(dev, qf, 0, &q);
    VkSubmitInfo sub = {VK_STRUCTURE_TYPE_SUBMIT_INFO}; sub.commandBufferCount = 1; sub.pCommandBuffers = &cmd;
    CHK(vkQueueSubmit(q, 1, &sub, VK_NULL_HANDLE));
    CHK(vkQueueWaitIdle(q));

    for (unsigned i = 0; i < nc; i++) {
        void *m; CHK(vkMapMemory(dev, rb[i].mem, 0, VK_WHOLE_SIZE, 0, &m));
        writeFile(cout[i], m, (size_t)width * height * cbytes[i]); vkUnmapMemory(dev, rb[i].mem);
    }
    if (hasDepth) {
        void *m; CHK(vkMapMemory(dev, drb.mem, 0, VK_WHOLE_SIZE, 0, &m));
        writeFile(dout, m, (size_t)width * height * 4); vkUnmapMemory(dev, drb.mem);
    }
    for (unsigned i = 0; i < ng; i++) if (gout[i][0]) {
        void *m; CHK(vkMapMemory(dev, gb[i].mem, 0, VK_WHOLE_SIZE, 0, &m));
        writeFile(gout[i], m, (size_t)gbytes[i]); vkUnmapMemory(dev, gb[i].mem);
    }
    return 0;
}
