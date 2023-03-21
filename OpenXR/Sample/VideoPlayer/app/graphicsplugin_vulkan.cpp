// Copyright (c) 2017-2022 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "common.h"
#include "geometry.h"
#include "graphicsplugin.h"
#include "options.h"

#ifdef XR_USE_GRAPHICS_API_VULKAN

#include <common/xr_linear.h>
#include <array>

namespace {

struct Vertex {
    XrVector3f Position;
    XrVector2f TexCoord;
};

VkFormat g_imageFormat = VK_FORMAT_R8_UNORM;

std::vector<Vertex> s_vertexCoordData = {
    {{-1.0f,  1.0f,  0.0f}, {0.0f, 0.0f}},
    {{ 1.0f,  1.0f,  0.0f}, {1.0f, 0.0f}},
    {{ 1.0f, -1.0f,  0.0f}, {1.0f, 1.0f}},
    {{-1.0f, -1.0f,  0.0f}, {0.0f, 1.0f}}
};

std::vector<uint16_t> s_indices = {
    0, 1, 2, 0, 2, 3
};

XrPosef Identity() {
    XrPosef t{};
    t.orientation.w = 1;
    return t;
}

XrPosef Translation(const XrVector3f& translation) {
    XrPosef t = Identity();
    t.position = translation;
    return t;
}

static std::string vkResultString(VkResult res) {
    switch (res) {
        case VK_SUCCESS:
            return "SUCCESS";
        case VK_NOT_READY:
            return "NOT_READY";
        case VK_TIMEOUT:
            return "TIMEOUT";
        case VK_EVENT_SET:
            return "EVENT_SET";
        case VK_EVENT_RESET:
            return "EVENT_RESET";
        case VK_INCOMPLETE:
            return "INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST:
            return "ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR:
            return "SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            return "ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT:
            return "ERROR_VALIDATION_FAILED_EXT";
        case VK_ERROR_INVALID_SHADER_NV:
            return "ERROR_INVALID_SHADER_NV";
        default:
            return std::to_string(res);
    }
}

[[noreturn]] inline void ThrowVkResult(VkResult res, const char* originator = nullptr, const char* sourceLocation = nullptr) {
    Throw(Fmt("VkResult failure [%s]", vkResultString(res).c_str()), originator, sourceLocation);
}
inline VkResult CheckVkResult(VkResult res, const char* originator = nullptr, const char* sourceLocation = nullptr) {
    if ((res) < VK_SUCCESS) {
        ThrowVkResult(res, originator, sourceLocation);
    }
    return res;
}
// XXX These really shouldn't have trailing ';'s
#define THROW_VK(res, cmd) ThrowVkResult(res, #cmd, FILE_AND_LINE);
#define CHECK_VKCMD(cmd) CheckVkResult(cmd, #cmd, FILE_AND_LINE);
#define CHECK_VKRESULT(res, cmdStr) CheckVkResult(res, cmdStr, FILE_AND_LINE);

struct MemoryAllocator {
    void Init(VkPhysicalDevice physicalDevice, VkDevice device) {
        m_vkDevice = device;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &m_memProps);
    }

    static const VkFlags defaultFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    void Allocate(VkMemoryRequirements const& memReqs, VkDeviceMemory* mem, VkFlags flags = defaultFlags,
                  const void* pNext = nullptr) const {
        // Search memtypes to find first index with those properties
        for (uint32_t i = 0; i < m_memProps.memoryTypeCount; ++i) {
            if ((memReqs.memoryTypeBits & (1 << i)) != 0u) {
                // Type is available, does it match user properties?
                if ((m_memProps.memoryTypes[i].propertyFlags & flags) == flags) {
                    VkMemoryAllocateInfo memAlloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, pNext};
                    memAlloc.allocationSize = memReqs.size;
                    memAlloc.memoryTypeIndex = i;
                    CHECK_VKCMD(vkAllocateMemory(m_vkDevice, &memAlloc, nullptr, mem));
                    return;
                }
            }
        }
        THROW("Memory format not supported");
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(m_vkDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_vkDevice, buffer, &memRequirements);

        Allocate(memRequirements, &bufferMemory, properties);

        vkBindBufferMemory(m_vkDevice, buffer, bufferMemory, 0);  
    }

   private:
    VkDevice m_vkDevice{VK_NULL_HANDLE};
    VkPhysicalDeviceMemoryProperties m_memProps{};
};

// CmdBuffer - manage VkCommandBuffer state
struct CmdBuffer {
#define LIST_CMDBUFFER_STATES(_) \
    _(Undefined)                 \
    _(Initialized)               \
    _(Recording)                 \
    _(Executable)                \
    _(Executing)
    enum class CmdBufferState {
#define MK_ENUM(name) name,
        LIST_CMDBUFFER_STATES(MK_ENUM)
#undef MK_ENUM
    };
    CmdBufferState state{CmdBufferState::Undefined};
    VkCommandPool pool{VK_NULL_HANDLE};
    VkCommandBuffer buf{VK_NULL_HANDLE};
    VkFence execFence{VK_NULL_HANDLE};
    VkQueue m_vkQueue{VK_NULL_HANDLE};

    CmdBuffer() = default;

    CmdBuffer(const CmdBuffer&) = delete;
    CmdBuffer& operator=(const CmdBuffer&) = delete;
    CmdBuffer(CmdBuffer&&) = delete;
    CmdBuffer& operator=(CmdBuffer&&) = delete;

    ~CmdBuffer() {
        SetState(CmdBufferState::Undefined);
        if (m_vkDevice != nullptr) {
            if (buf != VK_NULL_HANDLE) {
                vkFreeCommandBuffers(m_vkDevice, pool, 1, &buf);
            }
            if (pool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(m_vkDevice, pool, nullptr);
            }
            if (execFence != VK_NULL_HANDLE) {
                vkDestroyFence(m_vkDevice, execFence, nullptr);
            }
        }
        buf = VK_NULL_HANDLE;
        pool = VK_NULL_HANDLE;
        execFence = VK_NULL_HANDLE;
        m_vkDevice = nullptr;
    }

    std::string StateString(CmdBufferState s) {
        switch (s) {
#define MK_CASE(name)          \
    case CmdBufferState::name: \
        return #name;
            LIST_CMDBUFFER_STATES(MK_CASE)
#undef MK_CASE
        }
        return "(Unknown)";
    }

#define CHECK_CBSTATE(s)                                                                                           \
    do                                                                                                             \
        if (state != (s)) {                                                                                        \
            Log::Write(Log::Level::Error,                                                                          \
                       std::string("Expecting state " #s " from ") + __FUNCTION__ + ", in " + StateString(state)); \
            return false;                                                                                          \
        }                                                                                                          \
    while (0)

    bool Init(VkDevice device, uint32_t queueFamilyIndex, VkQueue vkqueue) {
        CHECK_CBSTATE(CmdBufferState::Undefined);

        m_vkDevice = device;
        m_vkQueue = vkqueue;

        // Create a command pool to allocate our command buffer from
        VkCommandPoolCreateInfo cmdPoolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
        CHECK_VKCMD(vkCreateCommandPool(m_vkDevice, &cmdPoolInfo, nullptr, &pool));

        // Create the command buffer from the command pool
        VkCommandBufferAllocateInfo cmd{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cmd.commandPool = pool;
        cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd.commandBufferCount = 1;
        CHECK_VKCMD(vkAllocateCommandBuffers(m_vkDevice, &cmd, &buf));

        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        CHECK_VKCMD(vkCreateFence(m_vkDevice, &fenceInfo, nullptr, &execFence));

        SetState(CmdBufferState::Initialized);
        return true;
    }

    VkCommandBuffer beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = pool;
        allocInfo.commandBufferCount = 1;
        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(m_vkDevice, &allocInfo, &commandBuffer);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        return commandBuffer;
    }

    void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        vkQueueSubmit(m_vkQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_vkQueue);
        vkFreeCommandBuffers(m_vkDevice, pool, 1, &commandBuffer);
    }

    bool Begin() {
        CHECK_CBSTATE(CmdBufferState::Initialized);
        VkCommandBufferBeginInfo cmdBeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        CHECK_VKCMD(vkBeginCommandBuffer(buf, &cmdBeginInfo));
        SetState(CmdBufferState::Recording);
        return true;
    }

    bool End() {
        CHECK_CBSTATE(CmdBufferState::Recording);
        CHECK_VKCMD(vkEndCommandBuffer(buf));
        SetState(CmdBufferState::Executable);
        return true;
    }

    bool Exec(VkQueue queue) {
        CHECK_CBSTATE(CmdBufferState::Executable);
        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &buf;
        CHECK_VKCMD(vkQueueSubmit(queue, 1, &submitInfo, execFence));
        SetState(CmdBufferState::Executing);
        return true;
    }

    bool Wait() {
        // Waiting on a not-in-flight command buffer is a no-op
        if (state == CmdBufferState::Initialized) {
            return true;
        }
        CHECK_CBSTATE(CmdBufferState::Executing);
        const uint32_t timeoutNs = 1 * 1000 * 1000 * 1000;
        for (int i = 0; i < 5; ++i) {
            auto res = vkWaitForFences(m_vkDevice, 1, &execFence, VK_TRUE, timeoutNs);
            if (res == VK_SUCCESS) {
                // Buffer can be executed multiple times...
                SetState(CmdBufferState::Executable);
                return true;
            }
            Log::Write(Log::Level::Info, "Waiting for CmdBuffer fence timed out, retrying...");
        }
        return false;
    }

    bool Reset() {
        if (state != CmdBufferState::Initialized) {
            CHECK_CBSTATE(CmdBufferState::Executable);
            CHECK_VKCMD(vkResetFences(m_vkDevice, 1, &execFence));
            CHECK_VKCMD(vkResetCommandBuffer(buf, 0));
            SetState(CmdBufferState::Initialized);
        }
        return true;
    }

   private:
    VkDevice m_vkDevice{VK_NULL_HANDLE};

    void SetState(CmdBufferState newState) { state = newState; }

#undef CHECK_CBSTATE
#undef LIST_CMDBUFFER_STATES
};

// ShaderProgram to hold a pair of vertex & fragment shaders
struct ShaderProgram {
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderInfo {{{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}, {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}}};

    ShaderProgram() = default;

    ~ShaderProgram() {
        if (m_vkDevice != nullptr) {
            for (auto& si : shaderInfo) {
                if (si.module != VK_NULL_HANDLE) {
                    vkDestroyShaderModule(m_vkDevice, shaderInfo[0].module, nullptr);
                }
                si.module = VK_NULL_HANDLE;
            }
        }
        shaderInfo = {};
        m_vkDevice = nullptr;
    }

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&&) = delete;
    ShaderProgram& operator=(ShaderProgram&&) = delete;

    void LoadVertexShader(const std::vector<uint32_t>& code) { Load(0, code); }
    void LoadFragmentShader(const std::vector<uint32_t>& code) { Load(1, code); }
    void Init(VkDevice device) { m_vkDevice = device; }

   private:
    VkDevice m_vkDevice{VK_NULL_HANDLE};
    void Load(uint32_t index, const std::vector<uint32_t>& code) {
        VkShaderModuleCreateInfo modInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        auto& si = shaderInfo[index];
        si.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        si.pName = "main";
        std::string name;
        switch (index) {
            case 0:
                si.stage = VK_SHADER_STAGE_VERTEX_BIT;
                name = "vertex";
                break;
            case 1:
                si.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                name = "fragment";
                break;
            default:
                THROW(Fmt("Unknown code index %d", index));
        }

        modInfo.codeSize = code.size() * sizeof(code[0]);
        modInfo.pCode = &code[0];
        CHECK_MSG((modInfo.codeSize > 0) && modInfo.pCode, Fmt("Invalid %s shader ", name.c_str()));

        CHECK_VKCMD(vkCreateShaderModule(m_vkDevice, &modInfo, nullptr, &si.module));

        Log::Write(Log::Level::Info, Fmt("Loaded %s shader", name.c_str()));
    }
};

// VertexBuffer base class
struct VertexBufferBase {
    VkBuffer indexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory indexBufferMemory{VK_NULL_HANDLE};
    VkBuffer vertexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory vertexBufferMemory{VK_NULL_HANDLE};
    VkVertexInputBindingDescription bindingDescription{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
    struct {
        uint32_t idx;
        uint32_t vtx;
    } count = {0, 0};

    VertexBufferBase() = default;

    ~VertexBufferBase() {
        if (m_vkDevice != nullptr) {
            if (indexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(m_vkDevice, indexBuffer, nullptr);
            }
            if (indexBufferMemory != VK_NULL_HANDLE) {
                vkFreeMemory(m_vkDevice, indexBufferMemory, nullptr);
            }
            if (vertexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(m_vkDevice, vertexBuffer, nullptr);
            }
            if (vertexBufferMemory != VK_NULL_HANDLE) {
                vkFreeMemory(m_vkDevice, vertexBufferMemory, nullptr);
            }
        }
        indexBuffer = VK_NULL_HANDLE;
        indexBufferMemory = VK_NULL_HANDLE;
        vertexBuffer = VK_NULL_HANDLE;
        vertexBufferMemory = VK_NULL_HANDLE;
        bindingDescription = {};
        attributeDescriptions.clear();
        count = {0, 0};
        m_vkDevice = nullptr;
    }

    VertexBufferBase(const VertexBufferBase&) = delete;
    VertexBufferBase& operator=(const VertexBufferBase&) = delete;
    VertexBufferBase(VertexBufferBase&&) = delete;
    VertexBufferBase& operator=(VertexBufferBase&&) = delete;
    void Init(VkDevice device, const MemoryAllocator* memAllocator, const std::vector<VkVertexInputAttributeDescription>& attr) {
        m_vkDevice = device;
        m_memAllocator = memAllocator;
        attributeDescriptions = attr;
    }

   protected:
    VkDevice m_vkDevice{VK_NULL_HANDLE};
    void AllocateBufferMemory(VkBuffer buf, VkDeviceMemory* mem) const {
        VkMemoryRequirements memReq = {};
        vkGetBufferMemoryRequirements(m_vkDevice, buf, &memReq);
        m_memAllocator->Allocate(memReq, mem);
    }

   private:
    const MemoryAllocator* m_memAllocator{nullptr};
};

// VertexBuffer template to wrap the indices and vertices
template <typename T>
struct VertexBuffer : public VertexBufferBase {
    bool Create(uint32_t idxCount, uint32_t vtxCount) {
        VkBufferCreateInfo bufInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        bufInfo.size = sizeof(uint16_t) * idxCount;
        CHECK_VKCMD(vkCreateBuffer(m_vkDevice, &bufInfo, nullptr, &indexBuffer));
        AllocateBufferMemory(indexBuffer, &indexBufferMemory);
        CHECK_VKCMD(vkBindBufferMemory(m_vkDevice, indexBuffer, indexBufferMemory, 0));

        bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufInfo.size = sizeof(T) * vtxCount;
        CHECK_VKCMD(vkCreateBuffer(m_vkDevice, &bufInfo, nullptr, &vertexBuffer));
        AllocateBufferMemory(vertexBuffer, &vertexBufferMemory);
        CHECK_VKCMD(vkBindBufferMemory(m_vkDevice, vertexBuffer, vertexBufferMemory, 0));

        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(T);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        count = {idxCount, vtxCount};

        return true;
    }

    void UpdateIndicies(const uint16_t* data, uint32_t elements, uint32_t offset = 0) {
        uint16_t* map = nullptr;
        CHECK_VKCMD(vkMapMemory(m_vkDevice, indexBufferMemory, sizeof(map[0]) * offset, sizeof(map[0]) * elements, 0, (void**)&map));
        for (size_t i = 0; i < elements; ++i) {
            map[i] = data[i];
        }
        vkUnmapMemory(m_vkDevice, indexBufferMemory);
    }

    void UpdateVertices(const T* data, uint32_t elements, uint32_t offset = 0) {
        T* map = nullptr;
        CHECK_VKCMD(vkMapMemory(m_vkDevice, vertexBufferMemory, sizeof(map[0]) * offset, sizeof(map[0]) * elements, 0, (void**)&map));
        for (size_t i = 0; i < elements; ++i) {
            map[i] = data[i];
        }
        vkUnmapMemory(m_vkDevice, vertexBufferMemory);
    }
};

// RenderPass wrapper
struct RenderPass {
    VkFormat colorFmt{};
    VkFormat depthFmt{};
    VkRenderPass pass{VK_NULL_HANDLE};

    RenderPass() = default;

    bool Create(VkDevice device, VkFormat aColorFmt, VkFormat aDepthFmt) {
        m_vkDevice = device;
        colorFmt = aColorFmt;
        depthFmt = aDepthFmt;
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        std::array<VkAttachmentDescription, 2> at = {};
        VkRenderPassCreateInfo rpInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpInfo.attachmentCount = 0;
        rpInfo.pAttachments = at.data();
        rpInfo.subpassCount = 1;
        rpInfo.pSubpasses = &subpass;

        if (colorFmt != VK_FORMAT_UNDEFINED) {
            colorRef.attachment = rpInfo.attachmentCount++;
            at[colorRef.attachment].format = colorFmt;
            at[colorRef.attachment].samples = VK_SAMPLE_COUNT_1_BIT;
            at[colorRef.attachment].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            at[colorRef.attachment].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            at[colorRef.attachment].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            at[colorRef.attachment].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            at[colorRef.attachment].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            at[colorRef.attachment].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorRef;
        }

        if (depthFmt != VK_FORMAT_UNDEFINED) {
            depthRef.attachment = rpInfo.attachmentCount++;
            at[depthRef.attachment].format = depthFmt;
            at[depthRef.attachment].samples = VK_SAMPLE_COUNT_1_BIT;
            at[depthRef.attachment].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            at[depthRef.attachment].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            at[depthRef.attachment].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            at[depthRef.attachment].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            at[depthRef.attachment].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            at[depthRef.attachment].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            subpass.pDepthStencilAttachment = &depthRef;
        }
        CHECK_VKCMD(vkCreateRenderPass(m_vkDevice, &rpInfo, nullptr, &pass));
        return true;
    }

    ~RenderPass() {
        if (m_vkDevice != nullptr) {
            if (pass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(m_vkDevice, pass, nullptr);
            }
        }
        pass = VK_NULL_HANDLE;
        m_vkDevice = nullptr;
    }

    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;
    RenderPass(RenderPass&&) = delete;
    RenderPass& operator=(RenderPass&&) = delete;

   private:
    VkDevice m_vkDevice{VK_NULL_HANDLE};
};

// VkImage + framebuffer wrapper
struct RenderTarget {
    VkImage colorImage{VK_NULL_HANDLE};
    VkImage depthImage{VK_NULL_HANDLE};
    VkImageView colorView{VK_NULL_HANDLE};
    VkImageView depthView{VK_NULL_HANDLE};
    VkFramebuffer fb{VK_NULL_HANDLE};

    RenderTarget() = default;

    ~RenderTarget() {
        if (m_vkDevice != nullptr) {
            if (fb != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(m_vkDevice, fb, nullptr);
            }
            if (colorView != VK_NULL_HANDLE) {
                vkDestroyImageView(m_vkDevice, colorView, nullptr);
            }
            if (depthView != VK_NULL_HANDLE) {
                vkDestroyImageView(m_vkDevice, depthView, nullptr);
            }
        }

        // Note we don't own color/depthImage, it will get destroyed when xrDestroySwapchain is called
        colorImage = VK_NULL_HANDLE;
        depthImage = VK_NULL_HANDLE;
        colorView = VK_NULL_HANDLE;
        depthView = VK_NULL_HANDLE;
        fb = VK_NULL_HANDLE;
        m_vkDevice = nullptr;
    }

    RenderTarget(RenderTarget&& other) noexcept : RenderTarget() {
        using std::swap;
        swap(colorImage, other.colorImage);
        swap(depthImage, other.depthImage);
        swap(colorView, other.colorView);
        swap(depthView, other.depthView);
        swap(fb, other.fb);
        swap(m_vkDevice, other.m_vkDevice);
    }
    RenderTarget& operator=(RenderTarget&& other) noexcept {
        if (&other == this) {
            return *this;
        }
        // Clean up ourselves.
        this->~RenderTarget();
        using std::swap;
        swap(colorImage, other.colorImage);
        swap(depthImage, other.depthImage);
        swap(colorView, other.colorView);
        swap(depthView, other.depthView);
        swap(fb, other.fb);
        swap(m_vkDevice, other.m_vkDevice);
        return *this;
    }
    void Create(VkDevice device, VkImage aColorImage, VkImage aDepthImage, VkExtent2D size, RenderPass& renderPass) {
        m_vkDevice = device;
        colorImage = aColorImage;
        depthImage = aDepthImage;
        std::array<VkImageView, 2> attachments{};
        uint32_t attachmentCount = 0;
        // Create color image view
        if (colorImage != VK_NULL_HANDLE) {
            VkImageViewCreateInfo colorViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            colorViewInfo.image = colorImage;
            colorViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            colorViewInfo.format = renderPass.colorFmt;
            colorViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
            colorViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
            colorViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
            colorViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
            colorViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            colorViewInfo.subresourceRange.baseMipLevel = 0;
            colorViewInfo.subresourceRange.levelCount = 1;
            colorViewInfo.subresourceRange.baseArrayLayer = 0;
            colorViewInfo.subresourceRange.layerCount = 1;
            CHECK_VKCMD(vkCreateImageView(m_vkDevice, &colorViewInfo, nullptr, &colorView));
            attachments[attachmentCount++] = colorView;
        }

        // Create depth image view
        if (depthImage != VK_NULL_HANDLE) {
            VkImageViewCreateInfo depthViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            depthViewInfo.image = depthImage;
            depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            depthViewInfo.format = renderPass.depthFmt;
            depthViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
            depthViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
            depthViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
            depthViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
            depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            depthViewInfo.subresourceRange.baseMipLevel = 0;
            depthViewInfo.subresourceRange.levelCount = 1;
            depthViewInfo.subresourceRange.baseArrayLayer = 0;
            depthViewInfo.subresourceRange.layerCount = 1;
            CHECK_VKCMD(vkCreateImageView(m_vkDevice, &depthViewInfo, nullptr, &depthView));
            attachments[attachmentCount++] = depthView;
        }

        VkFramebufferCreateInfo fbInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fbInfo.renderPass = renderPass.pass;
        fbInfo.attachmentCount = attachmentCount;
        fbInfo.pAttachments = attachments.data();
        fbInfo.width = size.width;
        fbInfo.height = size.height;
        fbInfo.layers = 1;
        CHECK_VKCMD(vkCreateFramebuffer(m_vkDevice, &fbInfo, nullptr, &fb));
    }

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

   private:
    VkDevice m_vkDevice{VK_NULL_HANDLE};
};

// vertex MVP xform & color fragment shader layout
struct PipelineLayout {
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSets;
    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMemory;
    void* uniformBufferMapped{nullptr};
    VkPhysicalDevice vkPhysicalDevice{VK_NULL_HANDLE};
    VkImage textureImage_y;
    VkImage textureImage_u;
    VkImage textureImage_v;
    VkImageView textureImageView_y;
    VkImageView textureImageView_u;
    VkImageView textureImageView_v;
    VkDeviceMemory textureImageMemory_y;
    VkDeviceMemory textureImageMemory_u;
    VkDeviceMemory textureImageMemory_v;
    VkSampler textureSampler_y;
    VkSampler textureSampler_u;
    VkSampler textureSampler_v;
    VkBuffer yuvBuffer_y;
    VkBuffer yuvBuffer_u;
    VkBuffer yuvBuffer_v;
    VkDeviceMemory yuvBufferMemory_y;
    VkDeviceMemory yuvBufferMemory_u;
    VkDeviceMemory yuvBufferMemory_v;
    void* yuvBufferMemoryMapped_y{nullptr};
    void* yuvBufferMemoryMapped_u{nullptr};
    void* yuvBufferMemoryMapped_v{nullptr};

    PipelineLayout() = default;

    ~PipelineLayout() {
        if (m_vkDevice != nullptr) {
            if (pipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(m_vkDevice, pipelineLayout, nullptr);
            }
            if (descriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(m_vkDevice, descriptorSetLayout, nullptr);
            }
            vkDestroyBuffer(m_vkDevice, yuvBuffer_y, nullptr);
            vkDestroyBuffer(m_vkDevice, yuvBuffer_u, nullptr);
            vkDestroyBuffer(m_vkDevice, yuvBuffer_v, nullptr);
            vkFreeMemory(m_vkDevice, textureImageMemory_y, nullptr);
            vkFreeMemory(m_vkDevice, textureImageMemory_u, nullptr);
            vkFreeMemory(m_vkDevice, textureImageMemory_v, nullptr);
            vkDestroyImage(m_vkDevice, textureImage_y, nullptr);
            vkDestroyImage(m_vkDevice, textureImage_u, nullptr);
            vkDestroyImage(m_vkDevice, textureImage_v, nullptr);
            vkDestroySampler(m_vkDevice, textureSampler_y, nullptr);
            vkDestroySampler(m_vkDevice, textureSampler_u, nullptr);
            vkDestroySampler(m_vkDevice, textureSampler_v, nullptr);
            vkFreeMemory(m_vkDevice, yuvBufferMemory_y, nullptr);
            vkFreeMemory(m_vkDevice, yuvBufferMemory_u, nullptr);
            vkFreeMemory(m_vkDevice, yuvBufferMemory_v, nullptr);
            vkDestroySampler(m_vkDevice, textureSampler_y, nullptr);
            vkDestroySampler(m_vkDevice, textureSampler_u, nullptr);
            vkDestroySampler(m_vkDevice, textureSampler_v, nullptr);
        }
        pipelineLayout = VK_NULL_HANDLE;
        descriptorSetLayout = VK_NULL_HANDLE;
        m_vkDevice = nullptr;
    }

    void Create(VkDevice device, MemoryAllocator* memAllocator, VkPhysicalDevice physicalDevice, int32_t videoWidth, int32_t videoHeight) {
        m_vkDevice = device;
        m_memAllocator = memAllocator;
        vkPhysicalDevice = physicalDevice;

        CreateUniformBuffer();
        CreateDescriptorPool();

        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.pImmutableSamplers = nullptr;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding samplerLayoutBinding_y{};
        samplerLayoutBinding_y.binding = 1;
        samplerLayoutBinding_y.descriptorCount = 1;
        samplerLayoutBinding_y.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding_y.pImmutableSamplers = nullptr;
        samplerLayoutBinding_y.stageFlags =  VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding samplerLayoutBinding_u{};
        samplerLayoutBinding_u.binding = 2;
        samplerLayoutBinding_u.descriptorCount = 1;
        samplerLayoutBinding_u.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding_u.pImmutableSamplers = nullptr;
        samplerLayoutBinding_u.stageFlags =  VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding samplerLayoutBinding_v{};
        samplerLayoutBinding_v.binding = 3;
        samplerLayoutBinding_v.descriptorCount = 1;
        samplerLayoutBinding_v.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding_v.pImmutableSamplers = nullptr;
        samplerLayoutBinding_v.stageFlags =  VK_SHADER_STAGE_FRAGMENT_BIT;        

        std::array<VkDescriptorSetLayoutBinding, 4> bindings = { uboLayoutBinding, samplerLayoutBinding_y, samplerLayoutBinding_u, samplerLayoutBinding_v };
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        CHECK_VKCMD(vkCreateDescriptorSetLayout(m_vkDevice, &layoutInfo, nullptr, &descriptorSetLayout));

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutCreateInfo.setLayoutCount = 1;
        pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
        CHECK_VKCMD(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

        CreateTextureImage(videoWidth, videoHeight);
        CreateTextureSampler();
        CreateDescriptorSets();
    }

    void CreateUniformBuffer() {
        VkDeviceSize bufferSize = sizeof(XrMatrix4x4f); //mvp
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        CHECK_VKCMD(vkCreateBuffer(m_vkDevice, &bufferInfo, nullptr, &uniformBuffer));

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_vkDevice, uniformBuffer, &memRequirements);
        m_memAllocator->Allocate(memRequirements, &uniformBufferMemory);
        vkBindBufferMemory(m_vkDevice, uniformBuffer, uniformBufferMemory, 0);
        vkMapMemory(m_vkDevice, uniformBufferMemory, 0, bufferSize, 0, &uniformBufferMapped);
    }

    void CreateDescriptorPool() {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 1;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = 3;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 1;
        CHECK_VKCMD(vkCreateDescriptorPool(m_vkDevice, &poolInfo, nullptr, &descriptorPool));
    }

    void CreateTextureImage(uint32_t width, uint32_t height) {
        m_memAllocator->createBuffer(width * height, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, yuvBuffer_y, yuvBufferMemory_y);
        m_memAllocator->createBuffer(width * height / 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, yuvBuffer_u, yuvBufferMemory_u);
        m_memAllocator->createBuffer(width * height / 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, yuvBuffer_v, yuvBufferMemory_v);
        vkMapMemory(m_vkDevice, yuvBufferMemory_y, 0, width * height, 0, &yuvBufferMemoryMapped_y);
        vkMapMemory(m_vkDevice, yuvBufferMemory_u, 0, width * height / 4, 0, &yuvBufferMemoryMapped_u);
        vkMapMemory(m_vkDevice, yuvBufferMemory_v, 0, width * height / 4, 0, &yuvBufferMemoryMapped_v);

        createImage(width, height, g_imageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage_y, textureImageMemory_y);
        createImage(width/2, height/2, g_imageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage_u, textureImageMemory_u);
        createImage(width/2, height/2, g_imageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage_v, textureImageMemory_v);

        textureImageView_y = createImageView(textureImage_y, g_imageFormat);
        textureImageView_u = createImageView(textureImage_u, g_imageFormat);
        textureImageView_v = createImageView(textureImage_v, g_imageFormat);
    }

    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        CHECK_VKCMD(vkCreateImage(m_vkDevice, &imageInfo, nullptr, &image));

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_vkDevice, image, &memRequirements);
        m_memAllocator->Allocate(memRequirements, &imageMemory, properties);
        vkBindImageMemory(m_vkDevice, image, imageMemory, 0);
    }

    VkImageView createImageView(VkImage image, VkFormat format) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        VkImageView imageView;
        CHECK_VKCMD(vkCreateImageView(m_vkDevice, &viewInfo, nullptr, &imageView));
        return imageView;
    }

    void CreateTextureSampler() {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.pNext = nullptr;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 1;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        CHECK_VKCMD(vkCreateSampler(m_vkDevice, &samplerInfo, nullptr, &textureSampler_y));
        CHECK_VKCMD(vkCreateSampler(m_vkDevice, &samplerInfo, nullptr, &textureSampler_u));
        CHECK_VKCMD(vkCreateSampler(m_vkDevice, &samplerInfo, nullptr, &textureSampler_v));
    }

    void CreateDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts{1, descriptorSetLayout};
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = layouts.data();
        CHECK_VKCMD(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSets));

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(XrMatrix4x4f);  //mvp

        VkDescriptorImageInfo imageInfo_y{};
        imageInfo_y.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo_y.imageView = textureImageView_y;
        imageInfo_y.sampler = textureSampler_y;

        VkDescriptorImageInfo imageInfo_u{};
        imageInfo_u.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo_u.imageView = textureImageView_u;
        imageInfo_u.sampler = textureSampler_u;

        VkDescriptorImageInfo imageInfo_v{};
        imageInfo_v.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo_v.imageView = textureImageView_v;
        imageInfo_v.sampler = textureSampler_v;

        std::array<VkWriteDescriptorSet, 4> descriptorWrites{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo_y;

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = descriptorSets;
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pImageInfo = &imageInfo_u;

        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = descriptorSets;
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pImageInfo = &imageInfo_v;

        vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    PipelineLayout(const PipelineLayout&) = delete;
    PipelineLayout& operator=(const PipelineLayout&) = delete;
    PipelineLayout(PipelineLayout&&) = delete;
    PipelineLayout& operator=(PipelineLayout&&) = delete;

   private:
    VkDevice m_vkDevice{VK_NULL_HANDLE};
    MemoryAllocator* m_memAllocator{nullptr};
};

// Pipeline wrapper for rendering pipeline state
struct Pipeline {
    VkPipeline graphicsPipeline{VK_NULL_HANDLE};
    VkPrimitiveTopology topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    std::vector<VkDynamicState> dynamicStateEnables;

    Pipeline() = default;

    void Dynamic(VkDynamicState state) { dynamicStateEnables.emplace_back(state); }

    void Create(VkDevice device, VkExtent2D size, const PipelineLayout& layout, const RenderPass& rp, const ShaderProgram& sp, const VertexBufferBase& vertexBuffer) {
        m_vkDevice = device;

        VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamicState.dynamicStateCount = (uint32_t)dynamicStateEnables.size();
        dynamicState.pDynamicStates = dynamicStateEnables.data();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &vertexBuffer.bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)vertexBuffer.attributeDescriptions.size();
        vertexInputInfo.pVertexAttributeDescriptions = vertexBuffer.attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssembly.primitiveRestartEnable = VK_FALSE;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0;
        rasterizer.depthBiasClamp = 0;
        rasterizer.depthBiasSlopeFactor = 0;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        VkRect2D scissor = {{0, 0}, size};
#if defined(ORIGIN_BOTTOM_LEFT)
        // Flipped view so origin is bottom-left like GL (requires VK_KHR_maintenance1)
        VkViewport viewport = {0.0f, (float)size.height, (float)size.width, -(float)size.height, 0.0f, 1.0f};
#else
        // Will invert y after projection
        VkViewport viewport = {0.0f, 0.0f, (float)size.width, (float)size.height, 0.0f, 1.0f};
#endif
        VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        ds.depthTestEnable = VK_TRUE;
        ds.depthWriteEnable = VK_TRUE;
        ds.depthCompareOp = VK_COMPARE_OP_LESS;
        ds.depthBoundsTestEnable = VK_FALSE;
        ds.stencilTestEnable = VK_FALSE;
        ds.front.failOp = VK_STENCIL_OP_KEEP;
        ds.front.passOp = VK_STENCIL_OP_KEEP;
        ds.front.depthFailOp = VK_STENCIL_OP_KEEP;
        ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
        ds.back = ds.front;
        ds.minDepthBounds = 0.0f;
        ds.maxDepthBounds = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = (uint32_t)sp.shaderInfo.size();
        pipelineInfo.pStages = sp.shaderInfo.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pTessellationState = nullptr;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        //pipelineInfo.pDepthStencilState = &ds;
        if (dynamicState.dynamicStateCount > 0) {
            pipelineInfo.pDynamicState = &dynamicState;
        }
        pipelineInfo.layout = layout.pipelineLayout;
        pipelineInfo.renderPass = rp.pass;
        pipelineInfo.subpass = 0;
        CHECK_VKCMD(vkCreateGraphicsPipelines(m_vkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline));
    }

    void Release() {
        if (m_vkDevice != nullptr) {
            if (graphicsPipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(m_vkDevice, graphicsPipeline, nullptr);
            }
        }
        graphicsPipeline = VK_NULL_HANDLE;
        m_vkDevice = nullptr;
    }

   private:
    VkDevice m_vkDevice{VK_NULL_HANDLE};
};

struct DepthBuffer {
    VkDeviceMemory depthMemory{VK_NULL_HANDLE};
    VkImage depthImage{VK_NULL_HANDLE};

    DepthBuffer() = default;

    ~DepthBuffer() {
        if (m_vkDevice != nullptr) {
            if (depthImage != VK_NULL_HANDLE) {
                vkDestroyImage(m_vkDevice, depthImage, nullptr);
            }
            if (depthMemory != VK_NULL_HANDLE) {
                vkFreeMemory(m_vkDevice, depthMemory, nullptr);
            }
        }
        depthImage = VK_NULL_HANDLE;
        depthMemory = VK_NULL_HANDLE;
        m_vkDevice = nullptr;
    }

    DepthBuffer(DepthBuffer&& other) noexcept : DepthBuffer() {
        using std::swap;
        swap(depthImage, other.depthImage);
        swap(depthMemory, other.depthMemory);
        swap(m_vkDevice, other.m_vkDevice);
    }
    DepthBuffer& operator=(DepthBuffer&& other) noexcept {
        if (&other == this) {
            return *this;
        }
        // clean up self
        this->~DepthBuffer();
        using std::swap;

        swap(depthImage, other.depthImage);
        swap(depthMemory, other.depthMemory);
        swap(m_vkDevice, other.m_vkDevice);
        return *this;
    }

    void Create(VkDevice device, MemoryAllocator* memAllocator, VkFormat depthFormat, const XrSwapchainCreateInfo& swapchainCreateInfo) {
        m_vkDevice = device;

        VkExtent2D size = {swapchainCreateInfo.width, swapchainCreateInfo.height};

        // Create a D32 depthbuffer
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = size.width;
        imageInfo.extent.height = size.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = depthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = (VkSampleCountFlagBits)swapchainCreateInfo.sampleCount;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        CHECK_VKCMD(vkCreateImage(device, &imageInfo, nullptr, &depthImage));

        VkMemoryRequirements memRequirements{};
        vkGetImageMemoryRequirements(device, depthImage, &memRequirements);
        memAllocator->Allocate(memRequirements, &depthMemory, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        CHECK_VKCMD(vkBindImageMemory(device, depthImage, depthMemory, 0));
    }

    void TransitionLayout(CmdBuffer* cmdBuffer, VkImageLayout newLayout) {
        if (newLayout == m_vkLayout) {
            return;
        }

        VkImageMemoryBarrier depthBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        depthBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        depthBarrier.oldLayout = m_vkLayout;
        depthBarrier.newLayout = newLayout;
        depthBarrier.image = depthImage;
        depthBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(cmdBuffer->buf, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0, nullptr, 1, &depthBarrier);

        m_vkLayout = newLayout;
    }

    DepthBuffer(const DepthBuffer&) = delete;
    DepthBuffer& operator=(const DepthBuffer&) = delete;

private:
    VkDevice m_vkDevice{VK_NULL_HANDLE};
    VkImageLayout m_vkLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct SwapchainImageContext {
    SwapchainImageContext(XrStructureType _swapchainImageType) : swapchainImageType(_swapchainImageType) {}

    // A packed array of XrSwapchainImageVulkan2KHR's for xrEnumerateSwapchainImages
    std::vector<XrSwapchainImageVulkan2KHR> swapchainImages;
    std::vector<RenderTarget> renderTarget;
    VkExtent2D size{};
    DepthBuffer depthBuffer{};
    RenderPass rp{};
    Pipeline pipeline{};
    XrStructureType swapchainImageType;

    SwapchainImageContext() = default;

    std::vector<XrSwapchainImageBaseHeader*> Create(VkDevice device, MemoryAllocator* memAllocator, uint32_t capacity,
                                                    const XrSwapchainCreateInfo& swapchainCreateInfo, const PipelineLayout& layout,
                                                    const ShaderProgram& sp, const VertexBuffer<Vertex>& vb) {
        m_vkDevice = device;
        size = {swapchainCreateInfo.width, swapchainCreateInfo.height};
        VkFormat colorFormat = (VkFormat)swapchainCreateInfo.format;
        VkFormat depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
        // XXX handle swapchainCreateInfo.sampleCount

        depthBuffer.Create(m_vkDevice, memAllocator, depthFormat, swapchainCreateInfo);
        rp.Create(m_vkDevice, colorFormat, depthFormat);
        pipeline.Create(m_vkDevice, size, layout, rp, sp, vb);

        swapchainImages.resize(capacity);
        renderTarget.resize(capacity);
        std::vector<XrSwapchainImageBaseHeader*> bases(capacity);
        for (uint32_t i = 0; i < capacity; ++i) {
            swapchainImages[i] = {swapchainImageType};
            bases[i] = reinterpret_cast<XrSwapchainImageBaseHeader*>(&swapchainImages[i]);
        }
        return bases;
    }

    uint32_t ImageIndex(const XrSwapchainImageBaseHeader* swapchainImageHeader) {
        auto p = reinterpret_cast<const XrSwapchainImageVulkan2KHR*>(swapchainImageHeader);
        return (uint32_t)(p - &swapchainImages[0]);
    }

    void BindRenderTarget(uint32_t index, VkRenderPassBeginInfo* renderPassBeginInfo) {
        if (renderTarget[index].fb == VK_NULL_HANDLE) {
            renderTarget[index].Create(m_vkDevice, swapchainImages[index].image, depthBuffer.depthImage, size, rp);
        }
        renderPassBeginInfo->renderPass = rp.pass;
        renderPassBeginInfo->framebuffer = renderTarget[index].fb;
        renderPassBeginInfo->renderArea.offset = {0, 0};
        renderPassBeginInfo->renderArea.extent = size;
    }

private:
    VkDevice m_vkDevice{VK_NULL_HANDLE};
};

struct VulkanGraphicsPlugin : public IGraphicsPlugin {
    VulkanGraphicsPlugin(const std::shared_ptr<Options>& options, std::shared_ptr<IPlatformPlugin> /*unused*/) {
        m_options = options;
        m_graphicsBinding.type = GetGraphicsBindingType();
    };

    std::vector<std::string> GetInstanceExtensions() const override { return {XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME}; }

    // Note: The output must not outlive the input - this modifies the input and returns a collection of views into that modified input!
    std::vector<const char*> ParseExtensionString(char* names) {
        std::vector<const char*> list;
        while (*names != 0) {
            list.push_back(names);
            while (*(++names) != 0) {
                if (*names == ' ') {
                    *names++ = '\0';
                    break;
                }
            }
        }
        return list;
    }

    const char* GetValidationLayerName() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        std::vector<const char*> validationLayerNames;
        validationLayerNames.push_back("VK_LAYER_KHRONOS_validation");
        validationLayerNames.push_back("VK_LAYER_LUNARG_standard_validation");

        // Enable only one validation layer from the list above. Prefer KHRONOS.
        for (auto& validationLayerName : validationLayerNames) {
            for (const auto& layerProperties : availableLayers) {
                if (0 == strcmp(validationLayerName, layerProperties.layerName)) {
                    return validationLayerName;
                }
            }
        }
        return nullptr;
    }

    void InitializeDevice(XrInstance instance, XrSystemId systemId) override {
        // Create the Vulkan device for the adapter associated with the system.
        // Extension function must be loaded by name
        XrGraphicsRequirementsVulkan2KHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
        CHECK_XRCMD(GetVulkanGraphicsRequirements2KHR(instance, systemId, &graphicsRequirements));

        VkResult err;
        std::vector<const char*> layers;
#if !defined(NDEBUG)
        const char* const validationLayerName = GetValidationLayerName();
        if (validationLayerName) {
            layers.push_back(validationLayerName);
        } else {
            Log::Write(Log::Level::Warning, "No validation layers found in the system, skipping");
        }
#endif

        std::vector<const char*> extensions;
        extensions.push_back("VK_EXT_debug_report");

        VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        appInfo.pApplicationName = "hello_xr";
        appInfo.applicationVersion = 1;
        appInfo.pEngineName = "hello_xr";
        appInfo.engineVersion = 1;
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo instInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        instInfo.pApplicationInfo = &appInfo;
        instInfo.enabledLayerCount = (uint32_t)layers.size();
        instInfo.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();
        instInfo.enabledExtensionCount = (uint32_t)extensions.size();
        instInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();

        XrVulkanInstanceCreateInfoKHR createInfo{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
        createInfo.systemId = systemId;
        createInfo.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
        createInfo.vulkanCreateInfo = &instInfo;
        createInfo.vulkanAllocator = nullptr;
        CHECK_XRCMD(CreateVulkanInstanceKHR(instance, &createInfo, &m_vkInstance, &err));
        CHECK_VKCMD(err);

        vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(m_vkInstance, "vkCreateDebugReportCallbackEXT");
        vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(m_vkInstance, "vkDestroyDebugReportCallbackEXT");
        VkDebugReportCallbackCreateInfoEXT debugInfo{VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT};
        debugInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
#if !defined(NDEBUG)
        debugInfo.flags |= VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
#endif
        debugInfo.pfnCallback = debugReportThunk;
        debugInfo.pUserData = this;
        CHECK_VKCMD(vkCreateDebugReportCallbackEXT(m_vkInstance, &debugInfo, nullptr, &m_vkDebugReporter));

        XrVulkanGraphicsDeviceGetInfoKHR deviceGetInfo{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
        deviceGetInfo.systemId = systemId;
        deviceGetInfo.vulkanInstance = m_vkInstance;
        CHECK_XRCMD(GetVulkanGraphicsDevice2KHR(instance, &deviceGetInfo, &m_vkPhysicalDevice));

        VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        float queuePriorities = 0;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriorities;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysicalDevice, &queueFamilyCount, &queueFamilyProps[0]);

        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            // Only need graphics (not presentation) for draw queue
            if ((queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u) {
                m_queueFamilyIndex = queueInfo.queueFamilyIndex = i;
                break;
            }
        }

        std::vector<const char*> deviceExtensions;

        VkPhysicalDeviceFeatures features{};
        // features.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledLayerCount = 0;
        deviceInfo.ppEnabledLayerNames = nullptr;
        deviceInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
        deviceInfo.ppEnabledExtensionNames = deviceExtensions.empty() ? nullptr : deviceExtensions.data();
        deviceInfo.pEnabledFeatures = &features;

        XrVulkanDeviceCreateInfoKHR deviceCreateInfo{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
        deviceCreateInfo.systemId = systemId;
        deviceCreateInfo.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
        deviceCreateInfo.vulkanCreateInfo = &deviceInfo;
        deviceCreateInfo.vulkanPhysicalDevice = m_vkPhysicalDevice;
        deviceCreateInfo.vulkanAllocator = nullptr;
        CHECK_XRCMD(CreateVulkanDeviceKHR(instance, &deviceCreateInfo, &m_vkDevice, &err));
        CHECK_VKCMD(err);

        vkGetDeviceQueue(m_vkDevice, queueInfo.queueFamilyIndex, 0, &m_vkQueue);

        m_memAllocator.Init(m_vkPhysicalDevice, m_vkDevice);

        InitializeResources();

        m_graphicsBinding.instance = m_vkInstance;
        m_graphicsBinding.physicalDevice = m_vkPhysicalDevice;
        m_graphicsBinding.device = m_vkDevice;
        m_graphicsBinding.queueFamilyIndex = queueInfo.queueFamilyIndex;
        m_graphicsBinding.queueIndex = 0;
    }

    void InitializeResources() {
        std::vector<uint32_t> vertexSPIRV = {
#include "vulkan_shaders/vert.spv"
        };
        std::vector<uint32_t> fragmentSPIRV = {
#include "vulkan_shaders/frag.spv"
        };
        if (vertexSPIRV.empty()) {THROW("Failed to compile vertex shader");}
        if (fragmentSPIRV.empty()) {THROW("Failed to compile fragment shader");}

        m_shaderProgram.Init(m_vkDevice);
        m_shaderProgram.LoadVertexShader(vertexSPIRV);
        m_shaderProgram.LoadFragmentShader(fragmentSPIRV);

        // Semaphore to block on draw complete
        VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        CHECK_VKCMD(vkCreateSemaphore(m_vkDevice, &semInfo, nullptr, &m_vkDrawDone));

        if (!m_cmdBuffer.Init(m_vkDevice, m_queueFamilyIndex, m_vkQueue)) {
            THROW("Failed to create command buffer");
        }

        if (m_videoWidth == 0 || m_videoHeight == 0) {THROW("video width or height error");}

        m_pipelineLayout.Create(m_vkDevice, &m_memAllocator, m_vkPhysicalDevice, m_videoWidth, m_videoHeight);

        m_drawBuffer.Init(m_vkDevice, &m_memAllocator,
                          {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Position)},
                           {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, TexCoord)}});


        if (m_options->VideoMode == "3D-SBS" || m_options->VideoMode == "2D") {
            XrVector3f scale{1.8, 1.0, 1.0};
            m_scale = scale;
            m_pose = Translation({0.f, 0.f, m_disdance});
        } else if (m_options->VideoMode == "360") {
            XrVector3f scale{1.0, 1.0, 1.0};
            m_scale = scale;
            m_pose = Translation({0.f, 0.f, 0.0f});
            calculateAttribute();
        }
        m_drawBuffer.Create(s_indices.size(), s_vertexCoordData.size());
        m_drawBuffer.UpdateVertices(s_vertexCoordData.data(), s_vertexCoordData.size(), 0);
        m_drawBuffer.UpdateIndicies(s_indices.data(), s_indices.size(), 0);
    }

    int64_t SelectColorSwapchainFormat(const std::vector<int64_t>& runtimeFormats) const override {
        // List of supported color swapchain formats.
        constexpr int64_t SupportedColorSwapchainFormats[] = {VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};
        auto swapchainFormatIt = std::find_first_of(runtimeFormats.begin(), runtimeFormats.end(), std::begin(SupportedColorSwapchainFormats), std::end(SupportedColorSwapchainFormats));
        if (swapchainFormatIt == runtimeFormats.end()) {
            THROW("No runtime swapchain format supported for color swapchain");
        }
        return *swapchainFormatIt;
    }

    const XrBaseInStructure* GetGraphicsBinding() const override {
        return reinterpret_cast<const XrBaseInStructure*>(&m_graphicsBinding);
    }

    std::vector<XrSwapchainImageBaseHeader*> AllocateSwapchainImageStructs(uint32_t capacity, const XrSwapchainCreateInfo& swapchainCreateInfo) override {
        // Allocate and initialize the buffer of image structs (must be sequential in memory for xrEnumerateSwapchainImages).
        // Return back an array of pointers to each swapchain image struct so the consumer doesn't need to know the type/size.
        // Keep the buffer alive by adding it into the list of buffers.
        m_swapchainImageContexts.emplace_back(GetSwapchainImageType());
        SwapchainImageContext& swapchainImageContext = m_swapchainImageContexts.back();
        std::vector<XrSwapchainImageBaseHeader*> bases = swapchainImageContext.Create(
            m_vkDevice, &m_memAllocator, capacity, swapchainCreateInfo, m_pipelineLayout, m_shaderProgram, m_drawBuffer);
        // Map every swapchainImage base pointer to this context
        for (auto& base : bases) {
            m_swapchainImageContextMap[base] = &swapchainImageContext;
        }
        return bases;
    }

    void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* swapchainImage,
                    int64_t /*swapchainFormat*/, const std::vector<Cube>& cubes) override {
    }

    void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* swapchainImage,
                        int64_t swapchainFormat, const std::shared_ptr<MediaFrame>& frame, const int32_t eye) override {
        CHECK(layerView.subImage.imageArrayIndex == 0);  // Texture arrays not supported.
        auto swapchainContext = m_swapchainImageContextMap[swapchainImage];
        uint32_t imageIndex = swapchainContext->ImageIndex(swapchainImage);
        m_cmdBuffer.Reset();
        m_cmdBuffer.Begin();
        // Ensure depth is in the right layout
        swapchainContext->depthBuffer.TransitionLayout(&m_cmdBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        // Bind and clear eye render target
        static XrColor4f darkSlateGrey = {m_backgroundColor[0], m_backgroundColor[1], m_backgroundColor[2], m_backgroundColor[3]};
        static std::array<VkClearValue, 2> clearValues;
        clearValues[0].color.float32[0] = darkSlateGrey.r;
        clearValues[0].color.float32[1] = darkSlateGrey.g;
        clearValues[0].color.float32[2] = darkSlateGrey.b;
        clearValues[0].color.float32[3] = darkSlateGrey.a;
        clearValues[1].depthStencil.depth = 1.0f;
        clearValues[1].depthStencil.stencil = 0;
        VkRenderPassBeginInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        renderPassInfo.clearValueCount = (uint32_t)clearValues.size();
        renderPassInfo.pClearValues = clearValues.data();

        swapchainContext->BindRenderTarget(imageIndex, &renderPassInfo);
        vkCmdBeginRenderPass(m_cmdBuffer.buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(m_cmdBuffer.buf, VK_PIPELINE_BIND_POINT_GRAPHICS, swapchainContext->pipeline.graphicsPipeline);

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(m_cmdBuffer.buf, 0, 1, &m_drawBuffer.vertexBuffer, &offset);

        //modify screen position
        m_pose.position.z = m_disdance;

        XrVector3f scale{1.f, 1.f, 1.f};
        const auto& pose = layerView.pose;
        XrMatrix4x4f proj;
        XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_VULKAN, layerView.fov, 0.05f, 100.0f);
        XrMatrix4x4f toView;
        XrMatrix4x4f_CreateTranslationRotationScale(&toView, &pose.position, &pose.orientation, &scale);
        XrMatrix4x4f view;
        XrMatrix4x4f_InvertRigidBody(&view, &toView);
        XrMatrix4x4f model;
        XrMatrix4x4f_CreateTranslationRotationScale(&model, &m_pose.position, &m_pose.orientation, &m_scale);
        XrMatrix4x4f vp;
        XrMatrix4x4f_Multiply(&vp, &proj, &view);
        XrMatrix4x4f mvp;
        XrMatrix4x4f_Multiply(&mvp, &vp, &model);
        // update uniformBuffer
        memcpy(m_pipelineLayout.uniformBufferMapped, &mvp, sizeof(mvp));

        if (m_options->VideoMode == "3D-SBS") {
            if (eye == 0) {
                s_vertexCoordData[0].TexCoord.x = 0.0f; s_vertexCoordData[0].TexCoord.y = 0.0f;
                s_vertexCoordData[1].TexCoord.x = 0.5f; s_vertexCoordData[1].TexCoord.y = 0.0f;
                s_vertexCoordData[2].TexCoord.x = 0.5f; s_vertexCoordData[2].TexCoord.y = 1.0f;
                s_vertexCoordData[3].TexCoord.x = 0.0f; s_vertexCoordData[3].TexCoord.y = 1.0f;
            } else {
                s_vertexCoordData[0].TexCoord.x = 0.5f; s_vertexCoordData[0].TexCoord.y = 0.0f;
                s_vertexCoordData[1].TexCoord.x = 1.0f; s_vertexCoordData[1].TexCoord.y = 0.0f;
                s_vertexCoordData[2].TexCoord.x = 1.0f; s_vertexCoordData[2].TexCoord.y = 1.0f;
                s_vertexCoordData[3].TexCoord.x = 0.5f; s_vertexCoordData[3].TexCoord.y = 1.0f;  
            }
            m_drawBuffer.UpdateVertices(s_vertexCoordData.data(), s_vertexCoordData.size(), 0);
        } else if (m_options->VideoMode == "360") {
        } else if (m_options->VideoMode == "2D") {
        }

        if (frame.get()) {
            uint32_t size_y = frame->width * frame->height;
            uint32_t size_u = size_y / 4;
            uint32_t size_v = size_y / 4;
            //copy yuv
            memcpy(m_pipelineLayout.yuvBufferMemoryMapped_y, frame->data, size_y);
            uint8_t *bufferu = (uint8_t*)m_pipelineLayout.yuvBufferMemoryMapped_u;
            for (int32_t i = size_y; i < frame->size; i += 2) {
                *bufferu++ = frame->data[i];
            }
            uint8_t *bufferv = (uint8_t*)m_pipelineLayout.yuvBufferMemoryMapped_v;
            for (int32_t i = size_y; i < frame->size; i += 2) {
                *bufferv++ = frame->data[i+1];
            }
            //copy image
            copyBufferToImage(m_pipelineLayout.yuvBuffer_y, m_pipelineLayout.textureImage_y, frame->width, frame->height);
            copyBufferToImage(m_pipelineLayout.yuvBuffer_u, m_pipelineLayout.textureImage_u, frame->width / 2, frame->height / 2);
            copyBufferToImage(m_pipelineLayout.yuvBuffer_v, m_pipelineLayout.textureImage_v, frame->width / 2, frame->height / 2);
        }

        vkCmdBindIndexBuffer(m_cmdBuffer.buf, m_drawBuffer.indexBuffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(m_cmdBuffer.buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout.pipelineLayout, 0, 1, &m_pipelineLayout.descriptorSets, 0, nullptr);
        vkCmdDrawIndexed(m_cmdBuffer.buf, m_drawBuffer.count.idx, 1, 0, 0, 0);

        vkCmdEndRenderPass(m_cmdBuffer.buf);
        m_cmdBuffer.End();
        m_cmdBuffer.Exec(m_vkQueue);
        m_cmdBuffer.Wait();
    };

    #define PI 3.1415926535
    #define RADIAN(x) ((x) * PI / 180)
    void calculateAttribute() {
        uint32_t vertexCount = 0;
        float angle_span = 1;
        int32_t index_width = 0;
        std::vector<Vertex> vertexCoordData;
        std::vector<uint16_t> indices;

        for (float vAngle = 0; vAngle <= 180; vAngle += angle_span) {      //vertical
            for (float hAngle = 0; hAngle <= 360; hAngle += angle_span) {
                float x = (float) m_radius * sin(RADIAN(vAngle)) * sin(RADIAN(hAngle));
                float y = (float) m_radius * cos(RADIAN(vAngle));
                float z = (float) m_radius * sin(RADIAN(vAngle)) * cos(RADIAN(hAngle));

                Vertex vertex{};
                vertex.Position.x = x;
                vertex.Position.y = y;
                vertex.Position.z = z;
                float textureCoords_x = 1 - hAngle / 360;
                float textureCoords_y = vAngle / 180;
                vertex.TexCoord.x = textureCoords_x;
                vertex.TexCoord.y = textureCoords_y;
                vertexCoordData.push_back(vertex);

                if (vAngle == angle_span && hAngle == 0) {
                    index_width = vertexCount;
                }
                if (vAngle > 0 && hAngle > 0) {
                    indices.push_back(vertexCount);
                    indices.push_back(vertexCount - index_width);
                    indices.push_back(vertexCount - index_width - 1);
                    indices.push_back(vertexCount);
                    indices.push_back(vertexCount - index_width - 1);
                    indices.push_back(vertexCount - 1);
                }
                vertexCount++;
            }
        }
        s_vertexCoordData = vertexCoordData;
        s_indices = indices;
        Log::Write(Log::Level::Error, Fmt("m_point:%d, vertexCount:%d, m_indicesCount:%d ", s_vertexCoordData.size(), vertexCount, s_indices.size()));
    }

    uint32_t GetSupportedSwapchainSampleCount(const XrViewConfigurationView&) override { return VK_SAMPLE_COUNT_1_BIT; }

    void SetVideoWidthHeight(int32_t videoWidth, int32_t videoHeight) override {
        m_videoHeight = videoHeight;
        m_videoWidth = videoWidth;
    };

    void SetInputAction(int hand /*0-left, 1-right*/, controllerInputAction &input) override {
        m_disdance += input.y * (-0.01f);
        if (m_disdance > -0.1f) {
            m_disdance = -0.1f;
        }

        float ratio = m_scale.x / m_scale.y;
        m_scale.x += input.x * 0.01f * ratio;
        m_scale.y += input.x * 0.01f;
    };

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer commandBuffer = m_cmdBuffer.beginSingleTimeCommands();
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { width, height, 1 };
        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        m_cmdBuffer.endSingleTimeCommands(commandBuffer);
    }

protected:
    XrGraphicsBindingVulkan2KHR m_graphicsBinding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
    std::list<SwapchainImageContext> m_swapchainImageContexts;
    std::map<const XrSwapchainImageBaseHeader*, SwapchainImageContext*> m_swapchainImageContextMap;

    VkInstance m_vkInstance{VK_NULL_HANDLE};
    VkPhysicalDevice m_vkPhysicalDevice{VK_NULL_HANDLE};
    VkDevice m_vkDevice{VK_NULL_HANDLE};
    uint32_t m_queueFamilyIndex = 0;
    VkQueue m_vkQueue{VK_NULL_HANDLE};
    VkSemaphore m_vkDrawDone{VK_NULL_HANDLE};

    MemoryAllocator m_memAllocator{};
    ShaderProgram m_shaderProgram{};
    CmdBuffer m_cmdBuffer{};
    PipelineLayout m_pipelineLayout{};
    VertexBuffer<Vertex> m_drawBuffer{};

    std::shared_ptr<Options> m_options;
    XrPosef m_pose = Translation({0.f, 0.f, -3.0f});
    XrVector3f m_scale{1.f, 1.f, 1.f};
    float m_radius = 50;
    int32_t m_videoWidth;
    int32_t m_videoHeight;
    float m_backgroundColor[4] = {0.01f, 0.01f, 0.01f, 1.0f};
    float m_disdance = -3.0f;

    PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT{nullptr};
    PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT{nullptr};
    VkDebugReportCallbackEXT m_vkDebugReporter{VK_NULL_HANDLE};

    VkBool32 debugReport(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t /*location*/,
                         int32_t /*messageCode*/, const char* pLayerPrefix, const char* pMessage) {
        std::string flagNames;
        std::string objName;
        Log::Level level = Log::Level::Error;

        if ((flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) != 0u) {
            flagNames += "DEBUG:";
            level = Log::Level::Verbose;
        }
        if ((flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) != 0u) {
            flagNames += "INFO:";
            level = Log::Level::Info;
        }
        if ((flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) != 0u) {
            flagNames += "PERF:";
            level = Log::Level::Warning;
        }
        if ((flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) != 0u) {
            flagNames += "WARN:";
            level = Log::Level::Warning;
        }
        if ((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) != 0u) {
            flagNames += "ERROR:";
            level = Log::Level::Error;
        }

#define LIST_OBJECT_TYPES(_) \
    _(UNKNOWN)               \
    _(INSTANCE)              \
    _(PHYSICAL_DEVICE)       \
    _(DEVICE)                \
    _(QUEUE)                 \
    _(SEMAPHORE)             \
    _(COMMAND_BUFFER)        \
    _(FENCE)                 \
    _(DEVICE_MEMORY)         \
    _(BUFFER)                \
    _(IMAGE)                 \
    _(EVENT)                 \
    _(QUERY_POOL)            \
    _(BUFFER_VIEW)           \
    _(IMAGE_VIEW)            \
    _(SHADER_MODULE)         \
    _(PIPELINE_CACHE)        \
    _(PIPELINE_LAYOUT)       \
    _(RENDER_PASS)           \
    _(PIPELINE)              \
    _(DESCRIPTOR_SET_LAYOUT) \
    _(SAMPLER)               \
    _(DESCRIPTOR_POOL)       \
    _(DESCRIPTOR_SET)        \
    _(FRAMEBUFFER)           \
    _(COMMAND_POOL)          \
    _(SURFACE_KHR)           \
    _(SWAPCHAIN_KHR)         \
    _(DISPLAY_KHR)           \
    _(DISPLAY_MODE_KHR)

        switch (objectType) {
            default:
#define MK_OBJECT_TYPE_CASE(name)                  \
    case VK_DEBUG_REPORT_OBJECT_TYPE_##name##_EXT: \
        objName = #name;                           \
        break;
                LIST_OBJECT_TYPES(MK_OBJECT_TYPE_CASE)

#if VK_HEADER_VERSION >= 46
                MK_OBJECT_TYPE_CASE(DESCRIPTOR_UPDATE_TEMPLATE_KHR)
#endif
#if VK_HEADER_VERSION >= 70
                MK_OBJECT_TYPE_CASE(DEBUG_REPORT_CALLBACK_EXT)
#endif
        }

        if ((objectType == VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT) && (strcmp(pLayerPrefix, "Loader Message") == 0) &&
            (strncmp(pMessage, "Device Extension:", 17) == 0)) {
            return VK_FALSE;
        }

        Log::Write(level, Fmt("%s (%s 0x%llx) [%s] %s", flagNames.c_str(), objName.c_str(), object, pLayerPrefix, pMessage));
        if ((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) != 0u) {
            return VK_FALSE;
        }
        if ((flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) != 0u) {
            return VK_FALSE;
        }
        return VK_FALSE;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportThunk(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
                                                           uint64_t object, size_t location, int32_t messageCode,
                                                           const char* pLayerPrefix, const char* pMessage, void* pUserData) {
        return static_cast<VulkanGraphicsPlugin*>(pUserData)->debugReport(flags, objectType, object, location, messageCode, pLayerPrefix, pMessage);
    }

    virtual XrStructureType GetGraphicsBindingType() const { return XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR; }
    virtual XrStructureType GetSwapchainImageType() const { return XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR; }

    virtual XrResult CreateVulkanInstanceKHR(XrInstance instance, const XrVulkanInstanceCreateInfoKHR* createInfo, VkInstance* vulkanInstance, VkResult* vulkanResult) {
        PFN_xrCreateVulkanInstanceKHR pfnCreateVulkanInstanceKHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrCreateVulkanInstanceKHR", reinterpret_cast<PFN_xrVoidFunction*>(&pfnCreateVulkanInstanceKHR)));
        return pfnCreateVulkanInstanceKHR(instance, createInfo, vulkanInstance, vulkanResult);
    }

    virtual XrResult CreateVulkanDeviceKHR(XrInstance instance, const XrVulkanDeviceCreateInfoKHR* createInfo, VkDevice* vulkanDevice, VkResult* vulkanResult) {
        PFN_xrCreateVulkanDeviceKHR pfnCreateVulkanDeviceKHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrCreateVulkanDeviceKHR", reinterpret_cast<PFN_xrVoidFunction*>(&pfnCreateVulkanDeviceKHR)));
        return pfnCreateVulkanDeviceKHR(instance, createInfo, vulkanDevice, vulkanResult);
    }

    virtual XrResult GetVulkanGraphicsDevice2KHR(XrInstance instance, const XrVulkanGraphicsDeviceGetInfoKHR* getInfo, VkPhysicalDevice* vulkanPhysicalDevice) {
        PFN_xrGetVulkanGraphicsDevice2KHR pfnGetVulkanGraphicsDevice2KHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsDevice2KHR", reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanGraphicsDevice2KHR)));
        return pfnGetVulkanGraphicsDevice2KHR(instance, getInfo, vulkanPhysicalDevice);
    }

    virtual XrResult GetVulkanGraphicsRequirements2KHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsVulkan2KHR* graphicsRequirements) {
        PFN_xrGetVulkanGraphicsRequirements2KHR pfnGetVulkanGraphicsRequirements2KHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirements2KHR", reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanGraphicsRequirements2KHR)));
        return pfnGetVulkanGraphicsRequirements2KHR(instance, systemId, graphicsRequirements);
    }
};

// A compatibility class that implements the KHR_vulkan_enable2 functionality on top of KHR_vulkan_enable
struct VulkanGraphicsPluginLegacy : public VulkanGraphicsPlugin {
    VulkanGraphicsPluginLegacy(const std::shared_ptr<Options>& options, std::shared_ptr<IPlatformPlugin> platformPlugin)
        : VulkanGraphicsPlugin(options, platformPlugin) {
        m_graphicsBinding.type = GetGraphicsBindingType();
    };

    std::vector<std::string> GetInstanceExtensions() const override { return {XR_KHR_VULKAN_ENABLE_EXTENSION_NAME}; }
    virtual XrStructureType GetGraphicsBindingType() const override { return XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR; }
    virtual XrStructureType GetSwapchainImageType() const override { return XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR; }

    virtual XrResult CreateVulkanInstanceKHR(XrInstance instance, const XrVulkanInstanceCreateInfoKHR* createInfo, VkInstance* vulkanInstance, VkResult* vulkanResult) override {
        PFN_xrGetVulkanInstanceExtensionsKHR pfnGetVulkanInstanceExtensionsKHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanInstanceExtensionsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanInstanceExtensionsKHR)));
        uint32_t extensionNamesSize = 0;
        CHECK_XRCMD(pfnGetVulkanInstanceExtensionsKHR(instance, createInfo->systemId, 0, &extensionNamesSize, nullptr));
        std::vector<char> extensionNames(extensionNamesSize);
        CHECK_XRCMD(pfnGetVulkanInstanceExtensionsKHR(instance, createInfo->systemId, extensionNamesSize, &extensionNamesSize, &extensionNames[0]));
        {
            // Note: This cannot outlive the extensionNames above, since it's just a collection of views into that string!
            std::vector<const char*> extensions = ParseExtensionString(&extensionNames[0]);
            // Merge the runtime's request with the applications requests
            for (uint32_t i = 0; i < createInfo->vulkanCreateInfo->enabledExtensionCount; ++i) {
                extensions.push_back(createInfo->vulkanCreateInfo->ppEnabledExtensionNames[i]);
            }
            VkInstanceCreateInfo instInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
            memcpy(&instInfo, createInfo->vulkanCreateInfo, sizeof(instInfo));
            instInfo.enabledExtensionCount = (uint32_t)extensions.size();
            instInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();
            auto pfnCreateInstance = (PFN_vkCreateInstance)createInfo->pfnGetInstanceProcAddr(nullptr, "vkCreateInstance");
            *vulkanResult = pfnCreateInstance(&instInfo, createInfo->vulkanAllocator, vulkanInstance);
        }
        return XR_SUCCESS;
    }

    virtual XrResult CreateVulkanDeviceKHR(XrInstance instance, const XrVulkanDeviceCreateInfoKHR* createInfo, VkDevice* vulkanDevice, VkResult* vulkanResult) override {
        PFN_xrGetVulkanDeviceExtensionsKHR pfnGetVulkanDeviceExtensionsKHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanDeviceExtensionsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanDeviceExtensionsKHR)));
        uint32_t deviceExtensionNamesSize = 0;
        CHECK_XRCMD(pfnGetVulkanDeviceExtensionsKHR(instance, createInfo->systemId, 0, &deviceExtensionNamesSize, nullptr));
        std::vector<char> deviceExtensionNames(deviceExtensionNamesSize);
        CHECK_XRCMD(pfnGetVulkanDeviceExtensionsKHR(instance, createInfo->systemId, deviceExtensionNamesSize, &deviceExtensionNamesSize, &deviceExtensionNames[0]));
        {
            // Note: This cannot outlive the extensionNames above, since it's just a collection of views into that string!
            std::vector<const char*> extensions = ParseExtensionString(&deviceExtensionNames[0]);
            // Merge the runtime's request with the applications requests
            for (uint32_t i = 0; i < createInfo->vulkanCreateInfo->enabledExtensionCount; ++i) {
                extensions.push_back(createInfo->vulkanCreateInfo->ppEnabledExtensionNames[i]);
            }
            VkPhysicalDeviceFeatures features{};
            memcpy(&features, createInfo->vulkanCreateInfo->pEnabledFeatures, sizeof(features));
#if !defined(XR_USE_PLATFORM_ANDROID)
            // Setting this quiets down a validation error triggered by the Oculus runtime
            features.shaderStorageImageMultisample = VK_TRUE;
#endif
            VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            memcpy(&deviceInfo, createInfo->vulkanCreateInfo, sizeof(deviceInfo));
            deviceInfo.pEnabledFeatures = &features;
            deviceInfo.enabledExtensionCount = (uint32_t)extensions.size();
            deviceInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();
            auto pfnCreateDevice = (PFN_vkCreateDevice)createInfo->pfnGetInstanceProcAddr(m_vkInstance, "vkCreateDevice");
            *vulkanResult = pfnCreateDevice(m_vkPhysicalDevice, &deviceInfo, createInfo->vulkanAllocator, vulkanDevice);
        }

        return XR_SUCCESS;
    }

    virtual XrResult GetVulkanGraphicsDevice2KHR(XrInstance instance, const XrVulkanGraphicsDeviceGetInfoKHR* getInfo, VkPhysicalDevice* vulkanPhysicalDevice) override {
        PFN_xrGetVulkanGraphicsDeviceKHR pfnGetVulkanGraphicsDeviceKHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsDeviceKHR", reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanGraphicsDeviceKHR)));
        if (getInfo->next != nullptr) {
            return XR_ERROR_FEATURE_UNSUPPORTED;
        }
        CHECK_XRCMD(pfnGetVulkanGraphicsDeviceKHR(instance, getInfo->systemId, getInfo->vulkanInstance, vulkanPhysicalDevice));
        return XR_SUCCESS;
    }

    virtual XrResult GetVulkanGraphicsRequirements2KHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsVulkan2KHR* graphicsRequirements) override {
        PFN_xrGetVulkanGraphicsRequirementsKHR pfnGetVulkanGraphicsRequirementsKHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanGraphicsRequirementsKHR)));
        XrGraphicsRequirementsVulkanKHR legacyRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
        CHECK_XRCMD(pfnGetVulkanGraphicsRequirementsKHR(instance, systemId, &legacyRequirements));
        graphicsRequirements->maxApiVersionSupported = legacyRequirements.maxApiVersionSupported;
        graphicsRequirements->minApiVersionSupported = legacyRequirements.minApiVersionSupported;
        return XR_SUCCESS;
    }
};

}  // namespace

std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_Vulkan(const std::shared_ptr<Options>& options, std::shared_ptr<IPlatformPlugin> platformPlugin) {
    return std::make_shared<VulkanGraphicsPlugin>(options, std::move(platformPlugin));
}

std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_VulkanLegacy(const std::shared_ptr<Options>& options, std::shared_ptr<IPlatformPlugin> platformPlugin) {
    return std::make_shared<VulkanGraphicsPluginLegacy>(options, std::move(platformPlugin));
}

#endif  // XR_USE_GRAPHICS_API_VULKAN
