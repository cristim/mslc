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
//   texture|storagetexture <metal index> <set> <binding> <w> <h> <rgba32float file>   (at most 4)
//   sampler <metal index> <set> <binding>                               (at most 4)
// A texture is a 2D RGBA32F sampled image (a storagetexture: a read-only storage image, which is
// what mslc declares for access::read) and a sampler is nearest and normalised, its address mode picked by the Metal index (0 clamp to edge, 1 repeat,
// 2 mirrored repeat), as in metalhost. Exit 0 on success, 2 on any Vulkan or file error (never a mismatch: this host
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

static void die(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
#include <stdarg.h>
static void die(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); fprintf(stderr, "vkhost: "); vfprintf(stderr, fmt, ap); fputc('\n', stderr);
    va_end(ap); exit(2);
}

static VkDevice dev;
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

#define MAXTEX 4
typedef struct { VkImage img; VkDeviceMemory mem; VkImageView view; } Img;
static Img makeImage(VkFormat f, unsigned w, unsigned h, VkImageUsageFlags usage, VkImageAspectFlags aspect);
static VkImageMemoryBarrier imgBarrier(VkImage img, VkImageAspectFlags aspect, VkImageLayout from, VkImageLayout to,
                                       VkAccessFlags src, VkAccessFlags dst);
static void fillBuf(Buf b, const void *data, size_t n);

// Nearest, normalised; the address mode is picked by the Metal sampler index, as in metalhost, so a
// swapped sampler binding changes what a coordinate outside [0, 1] reads: 0 clamp to edge,
// 1 repeat, 2 mirrored repeat.
static VkSampler makeNearestSampler(unsigned index) {
    VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = sci.minFilter = VK_FILTER_NEAREST; sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = sci.addressModeV = sci.addressModeW = index % 3 == 0 ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
        : index % 3 == 1 ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    VkSampler sampler; CHK(vkCreateSampler(dev, &sci, NULL, &sampler));
    return sampler;
}

// The image and the staging buffer holding the file's RGBA32F texels.
static Img loadTexture(const char *file, unsigned w, unsigned h, int storage, Buf *stage) {
    Img t = makeImage(VK_FORMAT_R32G32B32A32_SFLOAT, w, h, (storage ? VK_IMAGE_USAGE_STORAGE_BIT : VK_IMAGE_USAGE_SAMPLED_BIT) | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    size_t n; void *data = slurp(file, &n);
    if (n != (size_t)w * h * 16) die("%s has the wrong size", file);
    *stage = makeBuf(n, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 0); fillBuf(*stage, data, n); free(data);
    return t;
}

// Copies the staging buffer into the image and makes it readable by `consumer`.
static void recordTextureUpload(VkCommandBuffer cmd, Img t, Buf stage, unsigned w, unsigned h, int storage, VkPipelineStageFlags consumer) {
    VkImageMemoryBarrier to = imgBarrier(t.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &to);
    VkBufferImageCopy bc = {0, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {w, h, 1}};
    vkCmdCopyBufferToImage(cmd, stage.buf, t.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bc);
    VkImageMemoryBarrier rd = imgBarrier(t.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, storage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, consumer, 0, 0, NULL, 0, NULL, 1, &rd);
}

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
    unsigned tset[MAXTEX], tbind[MAXTEX], tw[MAXTEX], th[MAXTEX], nt = 0, sset[MAXTEX], sbind[MAXTEX], sidx[MAXTEX], ns = 0;
    char tfile[MAXTEX][1024]; int tstore[MAXTEX];
    FILE *mf = fopen(argv[1], "r");
    if (!mf) die("cannot open %s", argv[1]);
    char line[4096];
    while (fgets(line, sizeof line, mf)) {
        if (sscanf(line, "spv %1023s", spv) == 1 || sscanf(line, "entry %255s", entry) == 1) continue;
        if (sscanf(line, "set %u", &set) == 1) continue;
        if (sscanf(line, "local %u %u %u", local, local + 1, local + 2) == 3) continue;
        if (sscanf(line, "groups %u %u %u", groups, groups + 1, groups + 2) == 3) continue;
        if (nbuf < MAXBUF && sscanf(line, "buffer %u %llu %1023s %1023s", &member[nbuf], &bytes[nbuf], in[nbuf], out[nbuf]) == 4) { nbuf++; continue; }
        if (nt < MAXTEX && sscanf(line, "texture %*u %u %u %u %u %1023s", &tset[nt], &tbind[nt], &tw[nt], &th[nt], tfile[nt]) == 5) { tstore[nt++] = 0; continue; }
        if (nt < MAXTEX && sscanf(line, "storagetexture %*u %u %u %u %u %1023s", &tset[nt], &tbind[nt], &tw[nt], &th[nt], tfile[nt]) == 5) { tstore[nt++] = 1; continue; }
        if (ns < MAXTEX && sscanf(line, "sampler %u %u %u", &sidx[ns], &sset[ns], &sbind[ns]) == 3) { ns++; continue; }
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
    if (maxset >= 8) die("descriptor set %u too large", maxset);
    unsigned nsets = nbuf || nt || ns ? maxset + 1 : 0;
    VkDescriptorSetLayoutBinding lb[8][2 * MAXTEX + 1]; unsigned nlb[8] = {0};
    if (nbuf) lb[set][nlb[set]++] = (VkDescriptorSetLayoutBinding){0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL};
    for (unsigned i = 0; i < nt; i++) lb[tset[i]][nlb[tset[i]]++] = (VkDescriptorSetLayoutBinding){tbind[i], tstore[i] ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL};
    for (unsigned i = 0; i < ns; i++) lb[sset[i]][nlb[sset[i]]++] = (VkDescriptorSetLayoutBinding){sbind[i], VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL};
    VkDescriptorSetLayout dsl[8]; VkDescriptorSet dset[8];
    VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    Img tex[MAXTEX]; Buf stage[MAXTEX];
    if (nsets) {
        for (unsigned s = 0; s < nsets; s++) {
            VkDescriptorSetLayoutCreateInfo lci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            lci.bindingCount = nlb[s]; lci.pBindings = lb[s];
            CHK(vkCreateDescriptorSetLayout(dev, &lci, NULL, &dsl[s]));
        }
        plci.setLayoutCount = nsets; plci.pSetLayouts = dsl;
        VkDescriptorPoolSize ps[4] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAXTEX}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAXTEX}, {VK_DESCRIPTOR_TYPE_SAMPLER, MAXTEX}};
        VkDescriptorPoolCreateInfo pci = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pci.maxSets = nsets; pci.poolSizeCount = 4; pci.pPoolSizes = ps;
        VkDescriptorPool pool; CHK(vkCreateDescriptorPool(dev, &pci, NULL, &pool));
        VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dai.descriptorPool = pool; dai.descriptorSetCount = nsets; dai.pSetLayouts = dsl;
        CHK(vkAllocateDescriptorSets(dev, &dai, dset));
        VkWriteDescriptorSet wr[1 + 2 * MAXTEX]; unsigned nw = 0;
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
            tex[i] = loadTexture(tfile[i], tw[i], th[i], tstore[i], &stage[i]);
            ii[i] = (VkDescriptorImageInfo){VK_NULL_HANDLE, tex[i].view, tstore[i] ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            wr[nw] = (VkWriteDescriptorSet){VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wr[nw].dstSet = dset[tset[i]]; wr[nw].dstBinding = tbind[i]; wr[nw].descriptorCount = 1;
            wr[nw].descriptorType = tstore[i] ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE; wr[nw++].pImageInfo = &ii[i];
        }
        for (unsigned i = 0; i < ns; i++) {
            si[i] = (VkDescriptorImageInfo){makeNearestSampler(sidx[i]), VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
            wr[nw] = (VkWriteDescriptorSet){VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wr[nw].dstSet = dset[sset[i]]; wr[nw].dstBinding = sbind[i]; wr[nw].descriptorCount = 1;
            wr[nw].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER; wr[nw++].pImageInfo = &si[i];
        }
        if (nw) vkUpdateDescriptorSets(dev, nw, wr, 0, NULL);
    }
    VkPipelineLayout layout;
    CHK(vkCreatePipelineLayout(dev, &plci, NULL, &layout));

    size_t codeSize; uint32_t *code = slurp(spv, &codeSize);
    VkShaderModuleCreateInfo smci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = codeSize; smci.pCode = code;
    VkShaderModule mod; CHK(vkCreateShaderModule(dev, &smci, NULL, &mod));
    VkSpecializationMapEntry me[3] = {{0, 0, 4}, {1, 4, 4}, {2, 8, 4}};
    VkSpecializationInfo si = {3, me, sizeof local, local};
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
    for (unsigned i = 0; i < nt; i++) recordTextureUpload(cmd, tex[i], stage[i], tw[i], th[i], tstore[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    if (nsets) vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, nsets, dset, 0, NULL);
    vkCmdDispatch(cmd, groups[0], groups[1], groups[2]);
    VkMemoryBarrier mb = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0, NULL);
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
    return 0;
}

// ---- graphics: one fixed draw into offscreen targets (see tools/diffcheck/graphics.py) ----
//
// Manifest lines: mode draw, spv vertex|fragment FILE, vertex NAME, fragment NAME, target W H,
// draw V I, attr LOC FMT FILE, color I FMT C0 C1 C2 C3 OUT, depth CLEAR OUT,
// gbuffer STAGE INDEX SET MEMBER BYTES FILE, texture INDEX SET BINDING W H FILE,
// sampler INDEX SET BINDING. Same bytes and same names as metalhost's draw mode.
// The viewport has a negative height so framebuffer row 0 is the top, as in Metal.
// Read-back: colour/depth attachment write -> transfer read (image barrier), then transfer
// write -> host read, the barrier audited in PR #148.
#define MAXATTR 16
#define MAXCOLOR 8
#define MAXG 16

typedef struct { const char *name; VkFormat format; unsigned size; } Fmt;
static const Fmt VFMT[] = {
    {"float", VK_FORMAT_R32_SFLOAT, 4}, {"float2", VK_FORMAT_R32G32_SFLOAT, 8}, {"float3", VK_FORMAT_R32G32B32_SFLOAT, 12},
    {"float4", VK_FORMAT_R32G32B32A32_SFLOAT, 16}, {"int", VK_FORMAT_R32_SINT, 4}, {"int2", VK_FORMAT_R32G32_SINT, 8},
    {"int3", VK_FORMAT_R32G32B32_SINT, 12}, {"int4", VK_FORMAT_R32G32B32A32_SINT, 16}, {"uint", VK_FORMAT_R32_UINT, 4},
    {"uint2", VK_FORMAT_R32G32_UINT, 8}, {"uint3", VK_FORMAT_R32G32B32_UINT, 12}, {"uint4", VK_FORMAT_R32G32B32A32_UINT, 16},
    {"half2", VK_FORMAT_R16G16_SFLOAT, 4}, {"half3", VK_FORMAT_R16G16B16_SFLOAT, 6}, {"half4", VK_FORMAT_R16G16B16A16_SFLOAT, 8},
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

static uint32_t memTypeAny(uint32_t bits) {
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++) if (bits & (1u << i)) return i;
    die("no memory type");
}

static Img makeImage(VkFormat f, unsigned w, unsigned h, VkImageUsageFlags usage, VkImageAspectFlags aspect) {
    Img r = {0};
    VkImageCreateInfo ci = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = VK_IMAGE_TYPE_2D; ci.format = f; ci.extent = (VkExtent3D){w, h, 1}; ci.mipLevels = 1; ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT; ci.tiling = VK_IMAGE_TILING_OPTIMAL; ci.usage = usage;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    CHK(vkCreateImage(dev, &ci, NULL, &r.img));
    VkMemoryRequirements req; vkGetImageMemoryRequirements(dev, r.img, &req);
    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size; ai.memoryTypeIndex = memTypeAny(req.memoryTypeBits);
    CHK(vkAllocateMemory(dev, &ai, NULL, &r.mem));
    CHK(vkBindImageMemory(dev, r.img, r.mem, 0));
    VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = r.img; vi.viewType = VK_IMAGE_VIEW_TYPE_2D; vi.format = f;
    vi.subresourceRange = (VkImageSubresourceRange){aspect, 0, 1, 0, 1};
    CHK(vkCreateImageView(dev, &vi, NULL, &r.view));
    return r;
}

static void fillBuf(Buf b, const void *data, size_t n) {
    void *m; CHK(vkMapMemory(dev, b.mem, 0, VK_WHOLE_SIZE, 0, &m));
    memcpy(m, data, n); vkUnmapMemory(dev, b.mem);
}

static VkImageMemoryBarrier imgBarrier(VkImage img, VkImageAspectFlags aspect, VkImageLayout from, VkImageLayout to,
                                       VkAccessFlags src, VkAccessFlags dst) {
    VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcAccessMask = src; b.dstAccessMask = dst; b.oldLayout = from; b.newLayout = to;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; b.image = img;
    b.subresourceRange = (VkImageSubresourceRange){aspect, 0, 1, 0, 1};
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
    char gstage[MAXG][16], gfile[MAXG][1024]; unsigned gidx[MAXG], gset[MAXG], gmember[MAXG]; unsigned long long gbytes[MAXG];
    unsigned tset[4], tbind[4], tw[4], th[4]; char tfile[4][1024];
    unsigned sset[4], sbind[4], sidx[4];
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
        } else if (ng < MAXG && sscanf(line, "gbuffer %15s %u %u %u %llu %1023s", gstage[ng], &gidx[ng], &gset[ng], &gmember[ng],
                       &gbytes[ng], gfile[ng]) == 6) { ng++;
        } else if (nt < 4 && sscanf(line, "texture %*u %u %u %u %u %1023s", &tset[nt], &tbind[nt], &tw[nt], &th[nt], tfile[nt]) == 5) { nt++;
        } else if (ns < 4 && sscanf(line, "sampler %u %u %u", &sidx[ns], &sset[ns], &sbind[ns]) == 3) { ns++;
        } else die("bad manifest line: %s", line);
    }
    fclose(mf);
    if (!spv[0][0] || !spv[1][0] || !vname[0] || !fname[0] || !width || !height || !nverts) die("incomplete draw manifest");
    for (unsigned i = 0; i < nc; i++) if (cidx[i] != i) die("colour targets must be listed 0..n-1 in order");

    uint32_t qf = initDevice(VK_QUEUE_GRAPHICS_BIT);

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
    VkDescriptorSetLayoutBinding lb[8][8]; unsigned nlb[8] = {0};
    for (int st = 0; st < 2; st++) if (haveBlk[st]) {
        unsigned s = blkSet[st];
        lb[s][nlb[s]++] = (VkDescriptorSetLayoutBinding){0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
            st ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT, NULL};
    }
    for (unsigned i = 0; i < nt; i++) lb[tset[i]][nlb[tset[i]]++] = (VkDescriptorSetLayoutBinding){tbind[i], VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL};
    for (unsigned i = 0; i < ns; i++) lb[sset[i]][nlb[sset[i]]++] = (VkDescriptorSetLayoutBinding){sbind[i], VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL};
    VkDescriptorSetLayout dsl[8]; VkDescriptorSet dset[8];
    for (unsigned s = 0; s <= maxset; s++) {
        VkDescriptorSetLayoutCreateInfo lci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        lci.bindingCount = nlb[s]; lci.pBindings = lb[s];
        CHK(vkCreateDescriptorSetLayout(dev, &lci, NULL, &dsl[s]));
    }
    VkDescriptorPoolSize ps[3] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4}, {VK_DESCRIPTOR_TYPE_SAMPLER, 4}};
    VkDescriptorPoolCreateInfo dpi = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpi.maxSets = maxset + 1; dpi.poolSizeCount = 3; dpi.pPoolSizes = ps;
    VkDescriptorPool pool; CHK(vkCreateDescriptorPool(dev, &dpi, NULL, &pool));
    VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool = pool; dai.descriptorSetCount = maxset + 1; dai.pSetLayouts = dsl;
    CHK(vkAllocateDescriptorSets(dev, &dai, dset));
    VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = maxset + 1; plci.pSetLayouts = dsl;
    VkPipelineLayout layout; CHK(vkCreatePipelineLayout(dev, &plci, NULL, &layout));

    VkWriteDescriptorSet wr[16]; unsigned nw = 0;
    VkDescriptorBufferInfo bi[2]; VkDescriptorImageInfo ii[4], si[4];
    for (int st = 0; st < 2; st++) if (haveBlk[st]) {
        bi[st] = (VkDescriptorBufferInfo){blk[st].buf, 0, 8 * nmember[st]};
        wr[nw] = (VkWriteDescriptorSet){VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        wr[nw].dstSet = dset[blkSet[st]]; wr[nw].dstBinding = 0; wr[nw].descriptorCount = 1;
        wr[nw].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; wr[nw++].pBufferInfo = &bi[st];
    }
    Img tex[MAXTEX]; Buf stage[MAXTEX];
    for (unsigned i = 0; i < nt; i++) {
        tex[i] = loadTexture(tfile[i], tw[i], th[i], 0, &stage[i]);
        ii[i] = (VkDescriptorImageInfo){VK_NULL_HANDLE, tex[i].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        wr[nw] = (VkWriteDescriptorSet){VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        wr[nw].dstSet = dset[tset[i]]; wr[nw].dstBinding = tbind[i]; wr[nw].descriptorCount = 1;
        wr[nw].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE; wr[nw++].pImageInfo = &ii[i];
    }
    for (unsigned i = 0; i < ns; i++) {
        si[i] = (VkDescriptorImageInfo){makeNearestSampler(sidx[i]), VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
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
        color[i] = makeImage(f->format, width, height, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        rb[i] = makeBuf((size_t)width * height * f->size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0);
    }
    Img depth = {0}; Buf drb = {0};
    if (hasDepth) {
        depth = makeImage(VK_FORMAT_D32_SFLOAT, width, height, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
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
    for (unsigned i = 0; i < nc; i++) pre[npre++] = imgBarrier(color[i].img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    if (hasDepth) pre[npre++] = imgBarrier(depth.img, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
    if (npre) vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, npre, pre);
    for (unsigned i = 0; i < nt; i++) recordTextureUpload(cmd, tex[i], stage[i], tw[i], th[i], 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

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
    for (unsigned i = 0; i < nc; i++) post[npost++] = imgBarrier(color[i].img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    if (hasDepth) post[npost++] = imgBarrier(depth.img, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
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
    mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0, NULL);
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
    return 0;
}
