/************************************************************************************

Authors     :   Bradley Austin Davis <bdavis@saintandreas.org>
Copyright   :   Copyright Brad Davis. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

/************************************************************************************

Based on examples from the book Oculus Rift in Action

http://oculusriftinaction.com

************************************************************************************/

#include <iostream>
#include <memory>
#include <exception>
#include <vector>

//////////////////////////////////////////////////////////////////////
//
// The OVR types header contains the OS detection macros:
//  OVR_OS_WIN32, OVR_OS_MAC, OVR_OS_LINUX (and others)
//

#if (defined(WIN64) || defined(WIN32))
#define OS_WIN
#elif (defined(__APPLE__))
#define OS_OSX
#else
#define OS_LINUX
#endif

// Windows has a non-standard main function prototype
#ifdef OS_WIN
#define MAIN_DECL int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#else
#define MAIN_DECL int main(int argc, char ** argv)
#endif

#if defined(OS_WIN)
#define ON_WINDOWS(runnable) runnable()
#define ON_MAC(runnable)
#define ON_LINUX(runnable)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN 
#include <Windows.h>
#undef NOMINMAX
#elif defined(OS_OSX)
#define ON_WINDOWS(runnable) 
#define ON_MAC(runnable) runnable()
#define ON_LINUX(runnable)
#elif defined(OS_LINUX)
#define ON_WINDOWS(runnable) 
#define ON_MAC(runnable) 
#define ON_LINUX(runnable) runnable()
#endif


#define __STDC_FORMAT_MACROS 1
#define GLM_FORCE_RADIANS

#define FAIL(X) throw std::runtime_error(X)

#ifdef OVR_OS_LINUX
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#endif

///////////////////////////////////////////////////////////////////////////////
//
// GLM is a C++ math library meant to mirror the syntax of GLSL 
//

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

// Import the most commonly used types into the default namespace
using glm::ivec3;
using glm::ivec2;
using glm::uvec2;
using glm::mat3;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::quat;

///////////////////////////////////////////////////////////////////////////////
//
// GLEW gives cross platform access to OpenGL 3.x+ functionality.  
//

#include <GL/glew.h>
//////////////////////////////////////////////////////////////////////
//
// GLFW provides cross platform window creation
//

#include <GLFW/glfw3.h>

namespace glfw {
    inline uvec2 getSize(GLFWmonitor * monitor) {
        const GLFWvidmode * mode = glfwGetVideoMode(monitor);
        return uvec2(mode->width, mode->height);
    }

    inline ivec2 getPosition(GLFWmonitor * monitor) {
        ivec2 result;
        glfwGetMonitorPos(monitor, &result.x, &result.y);
        return result;
    }

    inline ivec2 getSecondaryScreenPosition(const uvec2 & size) {
        GLFWmonitor * primary = glfwGetPrimaryMonitor();
        int monitorCount;
        GLFWmonitor ** monitors = glfwGetMonitors(&monitorCount);
        GLFWmonitor * best = nullptr;
        uvec2 bestSize;
        for (int i = 0; i < monitorCount; ++i) {
            GLFWmonitor * cur = monitors[i];
            if (cur == primary) {
                continue;
            }
            uvec2 curSize = getSize(cur);
            if (best == nullptr || (bestSize.x < curSize.x && bestSize.y < curSize.y)) {
                best = cur;
                bestSize = curSize;
            }
        }
        if (nullptr == best) {
            best = primary;
            bestSize = getSize(best);
        }
        ivec2 pos = getPosition(best);
        if (bestSize.x > size.x) {
            pos.x += (bestSize.x - size.x) / 2;
        }

        if (bestSize.y > size.y) {
            pos.y += (bestSize.y - size.y) / 2;
        }

        return pos;
    }

    inline GLFWmonitor * getMonitorAtPosition(const ivec2 & position) {
        int count;
        GLFWmonitor ** monitors = glfwGetMonitors(&count);
        for (int i = 0; i < count; ++i) {
            ivec2 candidatePosition;
            glfwGetMonitorPos(monitors[i], &candidatePosition.x, &candidatePosition.y);
            if (candidatePosition == position) {
                return monitors[i];
            }
        }
        return nullptr;
    }

    inline GLFWwindow * createWindow(const uvec2 & size, const ivec2 & position = ivec2(INT_MIN), GLFWwindow * shared = nullptr) {
        GLFWwindow * window = glfwCreateWindow(size.x, size.y, "glfw", nullptr, shared);
        if (!window) {
            FAIL("Unable to create rendering window");
        }
        if ((position.x > INT_MIN) && (position.y > INT_MIN)) {
            glfwSetWindowPos(window, position.x, position.y);
        }
        return window;
    }

    inline GLFWwindow * createWindow(int w, int h, int x = INT_MIN, int y = INT_MIN, GLFWwindow * shared = nullptr) {
        return createWindow(uvec2(w, h), ivec2(x, y), shared);
    }

    inline GLFWwindow * createFullscreenWindow(const uvec2 & size, GLFWmonitor * targetMonitor, GLFWwindow * shared = nullptr) {
        return glfwCreateWindow(size.x, size.y, "glfw", targetMonitor, shared);
    }

    inline GLFWwindow * createSecondaryScreenWindow(const uvec2 & size, GLFWwindow * shared = nullptr) {
        return createWindow(size, glfw::getSecondaryScreenPosition(size), shared);
    }

}

extern "C" double ovr_GetTimeInSeconds();

class RateCounter {
    std::vector<float> times;

public:
    void reset() {
        times.clear();
    }

    float elapsed() const {
        if (times.size() < 1) {
            return 0.0f;
        }
        float elapsed = *times.rbegin() - *times.begin();
        return elapsed;
    }

    void increment() {
        times.push_back(ovr_GetTimeInSeconds());
    }

    float getRate() const {
        if (elapsed() == 0.0f) {
            return NAN;
        }
        return (times.size() - 1) / elapsed();
    }
};


//typedef void (APIENTRY *GLDEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, GLvoid* userParam);

void APIENTRY glDebugCallback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar * message,
    void * userParam) {
    const char * typeStr = "?";
    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        typeStr = "ERROR";
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        typeStr = "DEPRECATED_BEHAVIOR";
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        typeStr = "UNDEFINED_BEHAVIOR";
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        typeStr = "PORTABILITY";
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        typeStr = "PERFORMANCE";
        break;
    case GL_DEBUG_TYPE_OTHER:
        typeStr = "OTHER";
        break;
    }

    const char * severityStr = "?";
    switch (severity) {
    case GL_DEBUG_SEVERITY_LOW:
        severityStr = "LOW";
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        severityStr = "MEDIUM";
        break;
    case GL_DEBUG_SEVERITY_HIGH:
        severityStr = "HIGH";
        break;
    }
    OutputDebugStringA("--- OpenGL Callback Message ---\n");
    static char buffer[8192];
    sprintf(buffer, "type: %s\nseverity: %-8s\nid: %d\nmsg: %s\n", typeStr, severityStr, id,
        message);
    OutputDebugStringA(buffer);
    OutputDebugStringA("--- OpenGL Callback Message ---\n");
}

// A class to encapsulate using GLFW to handle input and render a scene
class GlfwApp {

protected:
    uvec2 windowSize;
    ivec2 windowPosition;
    GLFWwindow * window{ nullptr };
    unsigned int frame{ 0 };
    RateCounter rateCounter;
public:
    GlfwApp() {
        // Initialize the GLFW system for creating and positioning windows
        if (!glfwInit()) {
            FAIL("Failed to initialize GLFW");
        }
        glfwSetErrorCallback(ErrorCallback);
    }

    virtual ~GlfwApp() {
        if (nullptr != window) {
            glfwDestroyWindow(window);
        }
        glfwTerminate();
    }

    virtual int run() {
        preCreate();

        window = createRenderingTarget(windowSize, windowPosition);

        if (!window) {
            std::cout << "Unable to create OpenGL window" << std::endl;
            return -1;
        }

        postCreate();

        initGl();

        while (!glfwWindowShouldClose(window)) {
            ++frame;
            glfwPollEvents();
            update();
            draw();
            finishFrame();
            rateCounter.increment();
            if (rateCounter.elapsed() >= 5.0) {
                float fps = rateCounter.getRate();
                char message[8192];
                sprintf(message, "FPS: %0.2f\n", fps);
#ifdef OS_WIN
                OutputDebugStringA(message);
#else
		std::cout << message;
#endif
                rateCounter.reset();
            }
        }

        shutdownGl();

        return 0;
    }


protected:
    virtual GLFWwindow * createRenderingTarget(uvec2 & size, ivec2 & pos) = 0;

    virtual void draw() = 0;

    void preCreate() {
        glfwWindowHint(GLFW_DEPTH_BITS, 16);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        // Without this line we get
        // FATAL (86): NSGL: The targeted version of OS X only supports OpenGL 3.2 and later versions if they are forward-compatible
        ON_MAC([] {
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        });
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
    }

    void postCreate() {
        glfwSetWindowUserPointer(window, this);
        glfwSetKeyCallback(window, KeyCallback);
        glfwSetMouseButtonCallback(window, MouseButtonCallback);
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        // Initialize the OpenGL bindings
        // For some reason we have to set this experminetal flag to properly
        // init GLEW if we use a core context.
        glewExperimental = GL_TRUE;
        if (0 != glewInit()) {
            FAIL("Failed to initialize GLEW");
        }
        glGetError();

        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        GLuint unusedIds = 0;
        if (glDebugMessageCallback) {
            glDebugMessageCallback(glDebugCallback, this);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE,
                0, &unusedIds, true);
        } else if (glDebugMessageCallbackARB) {
            glDebugMessageCallbackARB(glDebugCallback, this);
            glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE,
                0, &unusedIds, true);
        }
    }

    virtual void initGl() {
    }

    virtual void shutdownGl() {

    }

    virtual void finishFrame() {
        glfwSwapBuffers(window);
    }

    virtual void destroyWindow() {
        glfwSetKeyCallback(window, nullptr);
        glfwSetMouseButtonCallback(window, nullptr);
        glfwDestroyWindow(window);
    }

    virtual void onKey(int key, int scancode, int action, int mods) {
        if (GLFW_PRESS != action) {
            return;
        }

        switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, 1);
            return;
        }
    }

    virtual void update() {}

    virtual void onMouseButton(int button, int action, int mods) {}

protected:
    virtual void viewport(const ivec2 & pos, const uvec2 & size) {
        glViewport(pos.x, pos.y, size.x, size.y);
    }

private:

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        GlfwApp * instance = (GlfwApp *)glfwGetWindowUserPointer(window);
        instance->onKey(key, scancode, action, mods);
    }

    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        GlfwApp * instance = (GlfwApp *)glfwGetWindowUserPointer(window);
        instance->onMouseButton(button, action, mods);
    }

    static void ErrorCallback(int error, const char* description) {
        FAIL(description);
    }
};



//////////////////////////////////////////////////////////////////////
//
// The Oculus VR C API provides access to information about the HMD
// and SDK based distortion.
//

// The oculus SDK requires it's own macros to distinguish between 
// operating systems, which is required to setup the platform specific
// values, like window handles
#if defined(OS_WIN)
#define OVR_OS_WIN32
#elif defined(OS_OSX)
#define OVR_OS_MAC
#elif defined(OS_LINUX)
#define OVR_OS_LINUX
#endif

#include <OVR_CAPI_GL.h>

namespace ovr {

    // Convenience method for looping over each eye with a lambda
    template <typename Function>
    inline void for_each_eye(Function function) {
        for (ovrEyeType eye = ovrEyeType::ovrEye_Left;
            eye < ovrEyeType::ovrEye_Count;
            eye = static_cast<ovrEyeType>(eye + 1)) {
            function(eye);
        }
    }

    inline mat4 toGlm(const ovrMatrix4f & om) {
        return glm::transpose(glm::make_mat4(&om.M[0][0]));
    }

    inline mat4 toGlm(const ovrFovPort & fovport, float nearPlane = 0.01f, float farPlane = 10000.0f) {
        return toGlm(ovrMatrix4f_Projection(fovport, nearPlane, farPlane, true));
    }

    inline vec3 toGlm(const ovrVector3f & ov) {
        return glm::make_vec3(&ov.x);
    }

    inline vec2 toGlm(const ovrVector2f & ov) {
        return glm::make_vec2(&ov.x);
    }

    inline uvec2 toGlm(const ovrSizei & ov) {
        return uvec2(ov.w, ov.h);
    }

    inline quat toGlm(const ovrQuatf & oq) {
        return glm::make_quat(&oq.x);
    }

    inline mat4 toGlm(const ovrPosef & op) {
        mat4 orientation = glm::mat4_cast(toGlm(op.Orientation));
        mat4 translation = glm::translate(mat4(), ovr::toGlm(op.Position));
        return translation * orientation;
    }

    inline ovrMatrix4f fromGlm(const mat4 & m) {
        ovrMatrix4f result;
        mat4 transposed(glm::transpose(m));
        memcpy(result.M, &(transposed[0][0]), sizeof(float) * 16);
        return result;
    }

    inline ovrVector3f fromGlm(const vec3 & v) {
        ovrVector3f result;
        result.x = v.x;
        result.y = v.y;
        result.z = v.z;
        return result;
    }

    inline ovrVector2f fromGlm(const vec2 & v) {
        ovrVector2f result;
        result.x = v.x;
        result.y = v.y;
        return result;
    }

    inline ovrSizei fromGlm(const uvec2 & v) {
        ovrSizei result;
        result.w = v.x;
        result.h = v.y;
        return result;
    }

    inline ovrQuatf fromGlm(const quat & q) {
        ovrQuatf result;
        result.x = q.x;
        result.y = q.y;
        result.z = q.z;
        result.w = q.w;
        return result;
    }


    inline GLFWwindow * createRiftRenderingWindow(ovrHmd hmd, glm::uvec2 & outSize, glm::ivec2 & outPosition, GLFWwindow * shared = nullptr) {
        outSize = glm::uvec2(hmd->Resolution.w, hmd->Resolution.h);
        outSize /= 4;
        GLFWwindow * window = glfw::createSecondaryScreenWindow(outSize, nullptr);
        glfwGetWindowPos(window, &outPosition.x, &outPosition.y);
        return window;
    }
}

class RiftManagerApp {
protected:
    ovrHmd hmd;
    uvec2 hmdNativeResolution;

public:
    RiftManagerApp() {
        ovrInitParams initParams; memset(&initParams, 0, sizeof(ovrInitParams));
        if (ovrSuccess != ovr_Initialize(nullptr)) {
            FAIL("Failed to initialize the Oculus SDK");
        }
    }

    virtual ~RiftManagerApp() {
        ovrHmd_Destroy(hmd);
        hmd = nullptr;
    }

    void initHmd(ovrHmdType defaultHmdType = ovrHmd_DK2) {
        if (ovrSuccess != ovrHmd_Create(0, &hmd)) {
            if (ovrSuccess != ovrHmd_CreateDebug(defaultHmdType, &hmd)) {
                FAIL("Could not create HMD");
            }
        }
        hmdNativeResolution = ivec2(hmd->Resolution.w, hmd->Resolution.h);
    }

    int getEnabledCaps() {
        return ovrHmd_GetEnabledCaps(hmd);
    }

    void enableCaps(int caps) {
        ovrHmd_SetEnabledCaps(hmd, getEnabledCaps() | caps);
    }

    void toggleCap(ovrHmdCaps cap) {
        if (cap & getEnabledCaps()) {
            disableCaps(cap);
        } else {
            enableCaps(cap);
        }
    }

    bool isEnabled(ovrHmdCaps cap) {
        return (cap == (cap & getEnabledCaps()));
    }

    void disableCaps(int caps) {
        ovrHmd_SetEnabledCaps(hmd, getEnabledCaps() & ~caps);
    }
};


#include <OVR_CAPI_GL.h>

struct RiftFramebufferWrapper {
    glm::uvec2 size;
    ovrSwapTextureSet* textureSet;
    GLuint fbo{ 0 };
    GLuint depth{ 0 };
    ovrHmd hmd;

    virtual ~RiftFramebufferWrapper() {
        release();
    }

private:
    void allocate() {
        ovrResult result = ovrHmd_CreateSwapTextureSetGL(hmd, GL_RGBA, size.x, size.y, &textureSet);
        assert(ovrSuccess == result);
        glGenRenderbuffers(1, &depth);
        assert(depth);
        glGenFramebuffers(1, &fbo);
        assert(fbo);
    }

    void release() {
        if (fbo) {
            glDeleteFramebuffers(1, &fbo);
            fbo = 0;
        }
        ovrHmd_DestroySwapTextureSet(hmd, textureSet);
        textureSet = nullptr;

        if (depth) {
            glDeleteRenderbuffers(1, &depth);
            depth = 0;
        }
    }

public:
    static bool checkStatus(GLenum target = GL_FRAMEBUFFER) {
        GLuint status = glCheckFramebufferStatus(target);
        switch (status) {
        case GL_FRAMEBUFFER_COMPLETE:
            return true;
            break;

        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            std::cerr << "framebuffer incomplete attachment" << std::endl;
            break;

        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            std::cerr << "framebuffer missing attachment" << std::endl;
            break;

        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            std::cerr << "framebuffer incomplete draw buffer" << std::endl;
            break;

        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            std::cerr << "framebuffer incomplete read buffer" << std::endl;
            break;

        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            std::cerr << "framebuffer incomplete multisample" << std::endl;
            break;

        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
            std::cerr << "framebuffer incomplete layer targets" << std::endl;
            break;

        case GL_FRAMEBUFFER_UNSUPPORTED:
            std::cerr << "framebuffer unsupported internal format or image" << std::endl;
            break;

        default:
            std::cerr << "other framebuffer error" << std::endl;
            break;
        }

        return false;
    }

    void init(ovrHmd hmd, const glm::ivec2 & size) {
        this->hmd = hmd;
        this->size = size;
        allocate();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        for (int i = 0; i < textureSet->TextureCount; ++i) {
            ovrGLTexture& tex = (ovrGLTexture&)(textureSet->Textures[i]);
            glBindTexture(GL_TEXTURE_2D, tex.OGL.TexId);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        if (depth) {
            glBindRenderbuffer(GL_RENDERBUFFER, depth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, size.x, size.y);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void activate() {
        assert(fbo);

        ovrGLTexture& tex = (ovrGLTexture&)(textureSet->Textures[textureSet->CurrentIndex]);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.OGL.TexId, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
        if (!checkStatus()) {
            FAIL("Invalid FBO");
        }
        glViewport(0, 0, size.x, size.y);
    }

    void deactivate() {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void increment() {
        ++textureSet->CurrentIndex;
        textureSet->CurrentIndex %= textureSet->TextureCount;
    }
};



class RiftApp : public GlfwApp, public RiftManagerApp {
public:

protected:
    ovrVector3f eyeOffsets[2];
//    ovrSwapTextureSet * eyeTextureSets[2];

private:
    ovrEyeType currentEye{ ovrEye_Count };
    ovrEyeRenderDesc eyeRenderDescs[2];
    mat4 projections[2];
    ovrPosef eyePoses[2];
    RiftFramebufferWrapper eyeFbos[2]; 
    ovrLayerEyeFov layer;
    int frameIndex{ 0 };

public:

    RiftApp() {
    }

    virtual void configureTracking() {
        if (ovrSuccess != ovrHmd_ConfigureTracking(hmd,
            ovrTrackingCap_Orientation | ovrTrackingCap_Position | ovrTrackingCap_MagYawCorrection, 0)) {
            FAIL("Could not attach to sensor device");
        }

    }

    virtual void configureRendering() {
        layer.Header.Type = ovrLayerType_EyeFov;
        layer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;
        ovr::for_each_eye([&](ovrEyeType eye) {
            ovrFovPort & fov = layer.Fov[eye] = hmd->MaxEyeFov[eye];
            ovrSizei & size = layer.Viewport[eye].Size = ovrHmd_GetFovTextureSize(hmd, eye, fov, 1.0f);
            layer.Viewport[eye].Pos = { 0, 0 };

            ovrEyeRenderDesc & erd = eyeRenderDescs[eye];
            erd = ovrHmd_GetRenderDesc(hmd, eye, hmd->MaxEyeFov[eye]);
            ovrMatrix4f ovrPerspectiveProjection = ovrMatrix4f_Projection(erd.Fov, OVR_DEFAULT_IPD * 4, 100000.0f, true);
            projections[eye] = ovr::toGlm(ovrPerspectiveProjection);
            eyeOffsets[eye] = erd.HmdToEyeViewOffset;

            // Allocate the frameBuffer that will hold the scene, and then be
            // re-rendered to the screen with distortion
            eyeFbos[eye].init(hmd, ovr::toGlm(size));
            layer.ColorTexture[eye] = eyeFbos[eye].textureSet;
        });
    }

    virtual ~RiftApp() {
    }

protected:
    virtual GLFWwindow * createRenderingTarget(uvec2 & outSize, ivec2 & outPosition) {
        initHmd();
        return ovr::createRiftRenderingWindow(hmd, outSize, outPosition, nullptr);
    }

    virtual void initGl() {
        GlfwApp::initGl();
        configureTracking();
        configureRendering();
    }

    // Override the base class to prevent the swap buffer call, because OVR does it in end frame
    virtual void finishFrame() {
    }

    virtual void onKey(int key, int scancode, int action, int mods) {
        if (GLFW_PRESS == action) switch (key) {
        case GLFW_KEY_R:
            ovrHmd_RecenterPose(hmd);
            return;
        }

        GlfwApp::onKey(key, scancode, action, mods);
    }

    virtual void draw() final {
        auto frameTiming = ovrHmd_GetFrameTiming(hmd, frameIndex);
        auto trackingState = ovrHmd_GetTrackingState(hmd, ovr_GetTimeInSeconds());
        ovr_CalcEyePoses(trackingState.HeadPose.ThePose, eyeOffsets, eyePoses);

        for (int i = 0; i < 2; ++i) {
            ovrEyeType eye = currentEye = hmd->EyeRenderOrder[i];
            // Render the scene to an offscreen buffer
            eyeFbos[eye].activate();
            renderScene(projections[eye], ovr::toGlm(eyePoses[eye]));
            eyeFbos[eye].deactivate();
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        ovrLayerHeader* layers = &layer.Header;
        ovr::for_each_eye([&](ovrEyeType eye) {
            layer.RenderPose[eye] = eyePoses[eye];
        });

        ovrHmd_SubmitFrame(hmd, frameIndex, nullptr, &layers, 1);

        ovr::for_each_eye([&](ovrEyeType eye) {
            eyeFbos[eye].increment();
        });
        ++frameIndex;
    }

    virtual void renderScene(const glm::mat4 & projection, const glm::mat4 & headPose) = 0;
};

//////////////////////////////////////////////////////////////////////
//
// The remainder of this code is specific to the scene we want to 
// render.  I use oglplus to render an array of cubes, but your 
// application would perform whatever rendering you want
//


//////////////////////////////////////////////////////////////////////
//
// OGLplus is a set of wrapper classes for giving OpenGL a more object
// oriented interface
//
#ifdef OVR_OS_WIN32
#pragma warning( disable : 4068 4244 4267 4065)
#endif
#include <oglplus/config/gl.hpp>
#include <oglplus/all.hpp>
#include <oglplus/interop/glm.hpp>
#include <oglplus/bound/texture.hpp>
#include <oglplus/bound/framebuffer.hpp>
#include <oglplus/bound/renderbuffer.hpp>
#include <oglplus/bound/buffer.hpp>
#include <oglplus/shapes/cube.hpp>
#include <oglplus/shapes/wrapper.hpp>
#ifdef OVR_OS_WIN32
#pragma warning( default : 4068 4244 4267 4065)
#endif

namespace Attribute {
    enum {
        Position = 0,
        TexCoord0 = 1,
        Normal = 2,
        Color = 3,
        TexCoord1 = 4,
        InstanceTransform = 5,
    };
}

static const char * VERTEX_SHADER =
"#version 410\n"

"uniform mat4 ProjectionMatrix;"
"uniform mat4 CameraMatrix;"

"layout(location = 0) in vec4 Position;"
"layout(location = 2) in vec3 Normal;"
"layout(location = 5) in mat4 InstanceTransform;"

"out vec3 vertNormal;"

"void main(void)"
"{"
" mat4 ViewXfm = CameraMatrix * InstanceTransform;"
" vertNormal = Normal;"
" gl_Position = ProjectionMatrix * ViewXfm * Position;"
"}";

static const char * FRAGMENT_SHADER =
"#version 410\n"
"in vec3 vertNormal;"

"out vec4 fragColor;"

"void main(void)"
"{"
" vec3 color = vertNormal;"
" if (!all(equal(color, abs(color)))) {"
"   color = vec3(1.0) - abs(color);"
" }"
" fragColor = vec4(color, 1.0);"
"}";

// a class for encapsulating building and rendering an RGB cube
struct ColorCubeScene {

    // Program
    oglplus::Program prog;
    oglplus::shapes::ShapeWrapper cube;
    GLuint instanceCount;
    oglplus::VertexArray vao;
    oglplus::Buffer instances;

    // VBOs for the cube's vertices and normals

    const unsigned int GRID_SIZE{ 5 };

public:
    ColorCubeScene() : cube({ "Position", "Normal" }, oglplus::shapes::Cube()) {
        using namespace oglplus;
        try {
            // attach the shaders to the program
            prog.AttachShader(
                FragmentShader()
                .Source(GLSLSource(String(FRAGMENT_SHADER)))
                .Compile()
                );
            prog.AttachShader(
                VertexShader()
                .Source(GLSLSource(String(VERTEX_SHADER)))
                .Compile()
                );
            prog.Link();
        } catch (ProgramBuildError & err) {
            FAIL((const char*)err.what());
        }

        // link and use it
        prog.Use();

        vao = cube.VAOForProgram(prog);
        vao.Bind();

        // Create a cube of cubes
        {
            std::vector<mat4> instance_positions;
            for (unsigned int z = 0; z < GRID_SIZE; ++z) {
                for (unsigned int y = 0; y < GRID_SIZE; ++y) {
                    for (unsigned int x = 0; x < GRID_SIZE; ++x) {
                        int xpos = (x - (GRID_SIZE / 2)) * 2;
                        int ypos = (y - (GRID_SIZE / 2)) * 2;
                        int zpos = z * -2;
                        vec3 relativePosition = vec3(xpos, ypos, zpos);
                        instance_positions.push_back(glm::translate(glm::mat4(1.0f), relativePosition));
                    }
                }
            }

            Context::Bound(Buffer::Target::Array, instances).Data(instance_positions);
            instanceCount = instance_positions.size();
            int stride = sizeof(mat4);
            for (int i = 0; i < 4; ++i) {
                VertexArrayAttrib instance_attr(prog, Attribute::InstanceTransform + i);
                size_t offset = sizeof(vec4) * i;
                instance_attr.Pointer(4, DataType::Float, false, stride, (void*)offset);
                instance_attr.Divisor(1);
                instance_attr.Enable();
            }
        }
    }

    void render(const mat4 & projection, const mat4 & modelview) {
        using namespace oglplus;
        prog.Use();
        typedef oglplus::Uniform<mat4> Mat4Uniform;
        Mat4Uniform(prog, "ProjectionMatrix").Set(projection);
        Mat4Uniform(prog, "CameraMatrix").Set(modelview);
        vao.Bind();
        cube.Draw(instanceCount);
    }
};


// An example application that renders a simple cube
class ExampleApp : public RiftApp {
    std::shared_ptr<ColorCubeScene> cubeScene;
    float ipd{ OVR_DEFAULT_IPD };
    mat4 modelview;

public:
    ExampleApp() {
        modelview = glm::lookAt(glm::vec3(0, 0, OVR_DEFAULT_IPD * 5.0f), glm::vec3(0), glm::vec3(0, 1, 0));
    }

protected:
    virtual void initGl() {
        RiftApp::initGl();
        glClearColor(0.2f, 0.2f, 0.2f, 0.0f);
        glClearDepth(1.0f);
        glDisable(GL_DITHER);
        glEnable(GL_DEPTH_TEST);
        ovrHmd_RecenterPose(hmd);
        cubeScene = std::shared_ptr<ColorCubeScene>(new ColorCubeScene());
    }

    virtual void shutdownGl() {
        cubeScene.reset();
    }

    void renderScene(const glm::mat4 & projection, const glm::mat4 & headPose) {
        // Clear the scene
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // apply the head pose to the current modelview matrix
        glm::mat4 modelview = glm::inverse(headPose) * this->modelview;
        // Scale the size of the cube to the distance between the eyes
        modelview = glm::scale(modelview, glm::vec3(ipd));
        cubeScene->render(projection, modelview);
    }
};

// Execute our example class
MAIN_DECL{
    int result = -1;
    try {
        result = ExampleApp().run();
    } catch (std::exception & error) {
        OutputDebugStringA(error.what());
        std::cerr << error.what() << std::endl;
    }
    ovr_Shutdown();
    return result;
}
