// Copyright (c) 2017-2022 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "common.h"
#include "geometry.h"
#include "graphicsplugin.h"
#include "options.h"
#include "main.h"

#ifdef XR_USE_GRAPHICS_API_OPENGL_ES

#include "common/gfxwrapper_opengl.h"
#include <common/xr_linear.h>

namespace {
constexpr float DarkSlateGray[] = {0.01f, 0.01f, 0.01f, 1.0f};

static const char* s_vertexShader = R"_(
    #version 320 es
    layout(location = 0) in vec3 aPosition;
    layout(location = 1) in vec2 aTexCoord;
    uniform mat4 ModelViewProjection;
    out vec2 vTexCoord;
    void main() {
        vTexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
        gl_Position = ModelViewProjection * vec4(aPosition.x, aPosition.y, aPosition.z, 1.0);
    }
)_";

static const char* s_fragmentShader_OES=R"_(
    #version 320 es
    #extension GL_OES_EGL_image_external_essl3:require
    precision mediump float;
    in vec2 vTexCoord;
    uniform samplerExternalOES yTexture;
    layout(location = 0) out vec4 outColor;
    void main() {
        vec4 texColor=texture(yTexture,vTexCoord);
        outColor=vec4(texColor.xyz,1.0);
    }
)_";

const GLfloat VERTICES_COORD[] = {
         // positions        //left textureCoords  //right textureCoords
         1.0f,  1.0f, 0.0f,    0.5f, 1.0f,       1.0f, 1.0f,  // top right  
         1.0f, -1.0f, 0.0f,    0.5f, 0.0f,       1.0f, 0.0f,  // bottom right 
        -1.0f, -1.0f, 0.0f,    0.0f, 0.0f,       0.5f, 0.0f,  // bottom left
        -1.0f,  1.0f, 0.0f,    0.0f, 1.0f,       0.5f, 1.0f   // top left
    };

const GLfloat VERTICES_COORD_OU[] = {  //over under
         // positions        //left textureCoords  //right textureCoords
         1.0f,  1.0f, 0.0f,    1.0f, 1.0f,       1.0f, 0.5f,  // top right  
         1.0f, -1.0f, 0.0f,    1.0f, 0.5f,       1.0f, 0.0f,  // bottom right 
        -1.0f, -1.0f, 0.0f,    0.0f, 0.5f,       0.0f, 0.0f,  // bottom left
        -1.0f,  1.0f, 0.0f,    0.0f, 1.0f,       0.0f, 0.5f   // top left
    };

const GLfloat VERTICES_COORD_2D[] = {
         // positions        // textureCoords
         1.0f,  1.0f, 0.0f,   1.0f, 1.0f,  // top right  
         1.0f, -1.0f, 0.0f,   1.0f, 0.0f,  // bottom right 
        -1.0f, -1.0f, 0.0f,   0.0f, 0.0f,  // bottom left
        -1.0f,  1.0f, 0.0f,   0.0f, 1.0f   // top left
    };

 const unsigned int s_indices[] = {
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
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

struct OpenGLESGraphicsPlugin : public IGraphicsPlugin {
    OpenGLESGraphicsPlugin(const std::shared_ptr<Options>& options, const std::shared_ptr<IPlatformPlugin> /*unused*/&) {
        m_options = options;
    };

    OpenGLESGraphicsPlugin(const OpenGLESGraphicsPlugin&) = delete;
    OpenGLESGraphicsPlugin& operator=(const OpenGLESGraphicsPlugin&) = delete;
    OpenGLESGraphicsPlugin(OpenGLESGraphicsPlugin&&) = delete;
    OpenGLESGraphicsPlugin& operator=(OpenGLESGraphicsPlugin&&) = delete;

    ~OpenGLESGraphicsPlugin() override {
        if (m_swapchainFramebuffer != 0) {
            glDeleteFramebuffers(1, &m_swapchainFramebuffer);
        }
        if (m_program != 0) {
            glDeleteProgram(m_program);
        }
        if (m_vao != 0) {
            glDeleteVertexArrays(1, &m_vao);
        }

        for (auto& colorToDepth : m_colorToDepthMap) {
            if (colorToDepth.second != 0) {
                glDeleteTextures(1, &colorToDepth.second);
            }
        }
    }

    std::vector<std::string> GetInstanceExtensions() const override { return {XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME}; }

    ksGpuWindow window{};

    void DebugMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message) {
        (void)source;
        (void)type;
        (void)id;
        (void)severity;
        Log::Write(Log::Level::Info, "GLES Debug: " + std::string(message, 0, length));
    }

    void InitializeDevice(XrInstance instance, XrSystemId systemId) override {
        // Extension function must be loaded by name
        PFN_xrGetOpenGLESGraphicsRequirementsKHR pfnGetOpenGLESGraphicsRequirementsKHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetOpenGLESGraphicsRequirementsKHR",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetOpenGLESGraphicsRequirementsKHR)));

        XrGraphicsRequirementsOpenGLESKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
        CHECK_XRCMD(pfnGetOpenGLESGraphicsRequirementsKHR(instance, systemId, &graphicsRequirements));

        // Initialize the gl extensions. Note we have to open a window.
        ksDriverInstance driverInstance{};
        ksGpuQueueInfo queueInfo{};
        ksGpuSurfaceColorFormat colorFormat{KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8};
        ksGpuSurfaceDepthFormat depthFormat{KS_GPU_SURFACE_DEPTH_FORMAT_D24};
        ksGpuSampleCount sampleCount{KS_GPU_SAMPLE_COUNT_1};
        if (!ksGpuWindow_Create(&window, &driverInstance, &queueInfo, 0, colorFormat, depthFormat, sampleCount, 640, 480, false)) {
            THROW("Unable to create GL context");
        }

        GLint major = 0;
        GLint minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);

        const XrVersion desiredApiVersion = XR_MAKE_VERSION(major, minor, 0);
        if (graphicsRequirements.minApiVersionSupported > desiredApiVersion) {
            THROW("Runtime does not support desired Graphics API and/or version");
        }

#if defined(XR_USE_PLATFORM_ANDROID)
        m_graphicsBinding.display = window.display;
        m_graphicsBinding.config = (EGLConfig)0;
        m_graphicsBinding.context = window.context.context;
#endif

        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(
            [](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message,
               const void* userParam) {
                ((OpenGLESGraphicsPlugin*)userParam)->DebugMessageCallback(source, type, id, severity, length, message);
            },
            this);

        InitializeResources();
    }

    void InitializeResources() {
        glGenFramebuffers(1, &m_swapchainFramebuffer);

        //vertex shader
        GLuint vertexShader_v2 = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader_v2, 1, &s_vertexShader, nullptr);
        glCompileShader(vertexShader_v2);
        CheckShader(vertexShader_v2);

        GLuint fragmentShader_v2 = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader_v2, 1, &s_fragmentShader_OES, nullptr);

        glCompileShader(fragmentShader_v2);
        CheckShader(fragmentShader_v2);

        m_program = glCreateProgram();
        glAttachShader(m_program, vertexShader_v2);
        glAttachShader(m_program, fragmentShader_v2);
        glLinkProgram(m_program);
        CheckProgram(m_program);
        glUseProgram(m_program);

        glDeleteShader(vertexShader_v2);
        glDeleteShader(fragmentShader_v2);

        GLuint apos = (GLuint) glGetAttribLocation(m_program, "aPosition");
        GLuint atex = (GLuint) glGetAttribLocation(m_program, "aTexCoord");
        GLuint texturey = (GLuint) glGetUniformLocation(m_program, "yTexture");
        m_modelViewProjectionUniformLocation = glGetUniformLocation(m_program, "ModelViewProjection");      

        glGenVertexArrays(1, &m_vao);
        unsigned int VBO, EBO;
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glBindVertexArray(m_vao);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glEnableVertexAttribArray(apos);
        glEnableVertexAttribArray(atex);

        if (m_options->VideoMode == "3D-SBS" || m_options->VideoMode == "3D-OU") {
            //set pose and scale when video mode is 3D-SBS or 3D-OU
            m_pose = Translation({0.f, 0.f, -3.0f});
            XrVector3f scale{1.8, 2.0, 1.0};
            m_scale = scale;
            if (m_options->VideoMode == "3D-SBS") {
                glBufferData(GL_ARRAY_BUFFER, sizeof(VERTICES_COORD), VERTICES_COORD, GL_STATIC_DRAW);
            } else if (m_options->VideoMode == "3D-OU") {
                glBufferData(GL_ARRAY_BUFFER, sizeof(VERTICES_COORD_OU), VERTICES_COORD_OU, GL_STATIC_DRAW);
            }
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(s_indices), s_indices, GL_STATIC_DRAW);
            glVertexAttribPointer(apos, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
            glVertexAttribPointer(atex, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
        } else if (m_options->VideoMode == "360") {
            //set pose and scale when video mode is 360
            m_pose = Translation({0.f, 0.f, 0.0f});
            XrVector3f scale{1.0, 1.0, 1.0};
            m_scale = scale;

            calculateAttribute();
            glBufferData(GL_ARRAY_BUFFER, m_vertexCoordData.size() * sizeof(float), m_vertexCoordData.data(), GL_STATIC_DRAW);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(GLuint), m_indices.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(apos, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
            glVertexAttribPointer(atex, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        } else if (m_options->VideoMode == "2D") {
            m_pose = Translation({0.f, 0.f, -3.0f});
            XrVector3f scale{1.8, 2.0, 1.0};
            m_scale = scale;

            glBufferData(GL_ARRAY_BUFFER, sizeof(VERTICES_COORD_2D), VERTICES_COORD_2D, GL_STATIC_DRAW);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(s_indices), s_indices, GL_STATIC_DRAW);
            glVertexAttribPointer(apos, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
            glVertexAttribPointer(atex, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        }

        glUniform1i(texturey, 0);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void CheckShader(GLuint shader) {
        GLint r = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &r);
        if (r == GL_FALSE) {
            GLchar msg[4096] = {};
            GLsizei length;
            glGetShaderInfoLog(shader, sizeof(msg), &length, msg);
            THROW(Fmt("Compile shader failed: %s", msg));
        }
    }

    void CheckProgram(GLuint prog) {
        GLint r = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &r);
        if (r == GL_FALSE) {
            GLchar msg[4096] = {};
            GLsizei length;
            glGetProgramInfoLog(prog, sizeof(msg), &length, msg);
            THROW(Fmt("Link program failed: %s", msg));
        }
    }

    int64_t SelectColorSwapchainFormat(const std::vector<int64_t>& runtimeFormats) const override {
        // List of supported color swapchain formats.
        constexpr int64_t SupportedColorSwapchainFormats[] = {
            GL_RGBA8,
            GL_RGBA8_SNORM,
        };

        auto swapchainFormatIt =
            std::find_first_of(runtimeFormats.begin(), runtimeFormats.end(), std::begin(SupportedColorSwapchainFormats),
                               std::end(SupportedColorSwapchainFormats));
        if (swapchainFormatIt == runtimeFormats.end()) {
            THROW("No runtime swapchain format supported for color swapchain");
        }

        return *swapchainFormatIt;
    }

    const XrBaseInStructure* GetGraphicsBinding() const override {
        return reinterpret_cast<const XrBaseInStructure*>(&m_graphicsBinding);
    }

    std::vector<XrSwapchainImageBaseHeader*> AllocateSwapchainImageStructs(
        uint32_t capacity, const XrSwapchainCreateInfo& /*swapchainCreateInfo*/) override {
        // Allocate and initialize the buffer of image structs (must be sequential in memory for xrEnumerateSwapchainImages).
        // Return back an array of pointers to each swapchain image struct so the consumer doesn't need to know the type/size.
        std::vector<XrSwapchainImageOpenGLESKHR> swapchainImageBuffer(capacity);
        std::vector<XrSwapchainImageBaseHeader*> swapchainImageBase;
        for (XrSwapchainImageOpenGLESKHR& image : swapchainImageBuffer) {
            image.type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
            swapchainImageBase.push_back(reinterpret_cast<XrSwapchainImageBaseHeader*>(&image));
        }

        // Keep the buffer alive by moving it into the list of buffers.
        m_swapchainImageBuffers.push_back(std::move(swapchainImageBuffer));

        return swapchainImageBase;
    }

    uint32_t GetDepthTexture(uint32_t colorTexture) {
        // If a depth-stencil view has already been created for this back-buffer, use it.
        auto depthBufferIt = m_colorToDepthMap.find(colorTexture);
        if (depthBufferIt != m_colorToDepthMap.end()) {
            return depthBufferIt->second;
        }

        // This back-buffer has no corresponding depth-stencil texture, so create one with matching dimensions.

        GLint width;
        GLint height;
        glBindTexture(GL_TEXTURE_2D, colorTexture);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

        uint32_t depthTexture;
        glGenTextures(1, &depthTexture);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);

        m_colorToDepthMap.insert(std::make_pair(colorTexture, depthTexture));

        return depthTexture;
    }

    void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* swapchainImage,
                    int64_t swapchainFormat, const std::vector<Cube>& cubes) override {
    }

    void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* swapchainImage,
                    int64_t swapchainFormat, const std::shared_ptr<MediaFrame>& frame, const int32_t eye) override {
        CHECK(layerView.subImage.imageArrayIndex == 0);  // Texture arrays not supported.
        UNUSED_PARM(swapchainFormat);                    // Not used in this function for now.

        glBindFramebuffer(GL_FRAMEBUFFER, m_swapchainFramebuffer);

        const uint32_t colorTexture = reinterpret_cast<const XrSwapchainImageOpenGLESKHR*>(swapchainImage)->image;

        glViewport(static_cast<GLint>(layerView.subImage.imageRect.offset.x),
                   static_cast<GLint>(layerView.subImage.imageRect.offset.y),
                   static_cast<GLsizei>(layerView.subImage.imageRect.extent.width),
                   static_cast<GLsizei>(layerView.subImage.imageRect.extent.height));

        glFrontFace(GL_CW);
        glCullFace(GL_BACK);
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        const uint32_t depthTexture = GetDepthTexture(colorTexture);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

        // Clear swapchain and depth buffer.
        glClearColor(DarkSlateGray[0], DarkSlateGray[1], DarkSlateGray[2], DarkSlateGray[3]);
        glClearDepthf(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glUseProgram(m_program);
        glBindVertexArray(m_vao);

        const auto& pose = layerView.pose;
        XrMatrix4x4f proj;
        XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_OPENGL_ES, layerView.fov, 0.05f, 100.0f);
        XrMatrix4x4f toView;
        XrVector3f scale{1.f, 1.f, 1.f};
        XrMatrix4x4f_CreateTranslationRotationScale(&toView, &pose.position, &pose.orientation, &scale);
        XrMatrix4x4f view;
        XrMatrix4x4f_InvertRigidBody(&view, &toView);
        XrMatrix4x4f vp;
        XrMatrix4x4f_Multiply(&vp, &proj, &view);

        //modify screen position
        m_pose.position.z = m_disdance;

        XrMatrix4x4f model;
        XrMatrix4x4f mvp;
        XrMatrix4x4f_CreateTranslationRotationScale(&model, &m_pose.position, &m_pose.orientation, &m_scale);
        XrMatrix4x4f_Multiply(&mvp, &vp, &model);

        
        {
            if (m_options->VideoMode == "3D-SBS" || m_options->VideoMode == "3D-OU") {
                GLuint aTexCoord = (GLuint) glGetAttribLocation(m_program, "aTexCoord");
                int32_t offset = 3 + (eye * 2);
                glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(offset * sizeof(float)));
                glEnableVertexAttribArray(aTexCoord);
            } else if (m_options->VideoMode == "2D" || m_options->VideoMode == "360") {
            }
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, gVideoGLTex->mGlTexture);

            glUniformMatrix4fv(m_modelViewProjectionUniformLocation, 1, GL_FALSE, reinterpret_cast<const GLfloat*>(&mvp));
            
            if (m_options->VideoMode == "3D-SBS" || m_options->VideoMode == "3D-OU" || m_options->VideoMode == "2D") {
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            } else if (m_options->VideoMode == "360") {
                glDrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, 0);
            }
        }

        glBindVertexArray(0);
        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Swap our window every other eye for RenderDoc
        static int everyOther = 0;
        if ((everyOther++ & 1) != 0) {
            ksGpuWindow_SwapBuffers(&window);
        }
    }
    //------------------------------------------------------------------
    #define PI 3.1415926535
    #define RADIAN(x) ((x) * PI / 180)
    void calculateAttribute() {
        m_vertexCount = 0;
        float angle_span = 1;
        int32_t index_width = 0;
        for (float vAngle = 0; vAngle <= 180; vAngle += angle_span) {      //vertical
            for (float hAngle = 0; hAngle <= 360; hAngle += angle_span) {
                float x = (float) m_radius * sin(RADIAN(vAngle)) * sin(RADIAN(hAngle));
                float y = (float) m_radius * cos(RADIAN(vAngle));
                float z = (float) m_radius * sin(RADIAN(vAngle)) * cos(RADIAN(hAngle));

                m_vertexCoordData.push_back(x);
                m_vertexCoordData.push_back(y);
                m_vertexCoordData.push_back(z);

                float textureCoords_x = 1 - hAngle / 360;
                float textureCoords_y = 1 - vAngle / 180;
                m_vertexCoordData.push_back(textureCoords_x);
                m_vertexCoordData.push_back(textureCoords_y);

                if (vAngle == angle_span && hAngle == 0) {
                    index_width = m_vertexCount;
                }
                if (vAngle > 0 && hAngle > 0) {
                    m_indices.push_back(m_vertexCount);
                    m_indices.push_back(m_vertexCount - index_width);
                    m_indices.push_back(m_vertexCount - index_width - 1);
                    m_indices.push_back(m_vertexCount);
                    m_indices.push_back(m_vertexCount - index_width - 1);
                    m_indices.push_back(m_vertexCount - 1);
                }
                m_vertexCount++;
            }
        }
        Log::Write(Log::Level::Error, Fmt("m_point:%d, m_vertexCount:%d, m_indicesCount:%d ", m_vertexCoordData.size(), m_vertexCount, m_indices.size()));
    }

    void SetInputAction(int hand /*0-left, 1-right*/, controllerInputAction &input) override {
        m_disdance += input.y * (-0.01f);
        if (m_disdance > -0.1f) {
            m_disdance = -0.1f;
        }

        float ratio = m_scale.x / m_scale.y;
        m_scale.x += input.x * 0.01f * ratio;
        m_scale.y += input.x * 0.01f;
    };

   private:
#ifdef XR_USE_PLATFORM_ANDROID
    XrGraphicsBindingOpenGLESAndroidKHR m_graphicsBinding{XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
#endif

    std::list<std::vector<XrSwapchainImageOpenGLESKHR>> m_swapchainImageBuffers;
    GLuint m_swapchainFramebuffer{0};
    GLuint m_program{0};
    GLint m_modelViewProjectionUniformLocation{0};
    GLuint m_vao{0};

    // Map color buffer to associated depth buffer. This map is populated on demand.
    std::map<uint32_t, uint32_t> m_colorToDepthMap;
    GLuint m_textureId[2];
    std::shared_ptr<Options> m_options;
    float m_radius = 50;
    uint32_t m_vertexCount;
    std::vector<float> m_vertexCoordData;
    std::vector<GLuint> m_indices;

    XrPosef m_pose = Translation({0.f, 0.f, -3.0f});
    XrVector3f m_scale{1.8, 1.0, 1.0};
    float m_disdance = -3.0f;
};
}  // namespace

std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_OpenGLES(const std::shared_ptr<Options>& options, std::shared_ptr<IPlatformPlugin> platformPlugin) {
    return std::make_shared<OpenGLESGraphicsPlugin>(options, platformPlugin);
}

#endif
