// Copyright (c) 2017-2022, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "common.h"
#include "options.h"
#include "platformdata.h"
#include "platformplugin.h"
#include "graphicsplugin.h"
#include "openxr_program.h"
#include "main.h"

JavaVM* gVm = NULL;
JNIEnv* gEnv = NULL;
ANativeWindow* gNativeWindow=NULL;

jobject gVideoSurfaceJObj;  
jmethodID gNewSurfaceAndTexMID;
jmethodID gUpdateTexImageMID;

VideoGLTex* gVideoGLTex;
JNIEnv* gCPPEnv = NULL;     

extern "C" {
void Java_com_khronos_player_VideoSurface_setSurface(JNIEnv *env,jclass clazz,jobject surface)
{
    gNativeWindow=ANativeWindow_fromSurface(env,surface);
}

jint JNI_OnLoad(JavaVM *vm, void *)
{
    gVm = vm;

    if (vm->GetEnv((void**) &gEnv, JNI_VERSION_1_6) != JNI_OK)
    {
        Log::Write(Log::Level::Error,"Error JNI_OnLoad vm->GetEnv Error");
        return -1;
    }
    const jclass VideoSurface_Class=gEnv->FindClass("com/khronos/player/VideoSurface");
    const jmethodID constructor=gEnv->GetMethodID(VideoSurface_Class,"<init>","()V");

    jobject object=gEnv->NewObject(VideoSurface_Class,constructor);

    gVideoSurfaceJObj=gEnv->NewGlobalRef(object);

    gNewSurfaceAndTexMID=gEnv->GetMethodID(VideoSurface_Class,"newSurfaceAndTex","(I)V");
    gUpdateTexImageMID=gEnv->GetMethodID(VideoSurface_Class,"curSurfaceTexUpdate","()V");
    return JNI_VERSION_1_6;
}


bool UpdateOptionsFromSystemProperties(Options& options) {
    char value[PROP_VALUE_MAX] = {};
    if (__system_property_get("debug.xr.graphicsPlugin", value) != 0) {
        options.GraphicsPlugin = value;
    }
    // Check for required parameters.
    if (options.GraphicsPlugin.empty()) {
        Log::Write(Log::Level::Warning, "GraphicsPlugin Default OpenGLES");
        options.GraphicsPlugin = "OpenGLES";
    }
    return true;
}

struct AndroidAppState {
    ANativeWindow* NativeWindow = nullptr;
    bool Resumed = false;
};

/**
 * Process the next main command.
 */
static void app_handle_cmd(struct android_app* app, int32_t cmd) {
    AndroidAppState* appState = (AndroidAppState*)app->userData;
    switch (cmd) {
        // There is no APP_CMD_CREATE. The ANativeActivity creates the
        // application thread from onCreate(). The application thread
        // then calls android_main().
        case APP_CMD_START: {
            Log::Write(Log::Level::Info, "    APP_CMD_START");
            Log::Write(Log::Level::Info, "onStart()");
            break;
        }
        case APP_CMD_RESUME: {
            Log::Write(Log::Level::Info, "onResume()");
            Log::Write(Log::Level::Info, "    APP_CMD_RESUME");
            appState->Resumed = true;
            break;
        }
        case APP_CMD_PAUSE: {
            Log::Write(Log::Level::Info, "onPause()");
            Log::Write(Log::Level::Info, "    APP_CMD_PAUSE");
            appState->Resumed = false;
            break;
        }
        case APP_CMD_STOP: {
            Log::Write(Log::Level::Info, "onStop()");
            Log::Write(Log::Level::Info, "    APP_CMD_STOP");
            break;
        }
        case APP_CMD_DESTROY: {
            Log::Write(Log::Level::Info, "onDestroy()");
            Log::Write(Log::Level::Info, "    APP_CMD_DESTROY");
            appState->NativeWindow = NULL;
            break;
        }
        case APP_CMD_INIT_WINDOW: {
            Log::Write(Log::Level::Info, "surfaceCreated()");
            Log::Write(Log::Level::Info, "    APP_CMD_INIT_WINDOW");
            appState->NativeWindow = app->window;
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            Log::Write(Log::Level::Info, "surfaceDestroyed()");
            Log::Write(Log::Level::Info, "    APP_CMD_TERM_WINDOW");
            appState->NativeWindow = NULL;
            break;
        }
    }
}

void android_main(struct android_app* app){
    try {
        app->activity->vm->AttachCurrentThread(&gCPPEnv, nullptr);

        AndroidAppState appState = {};

        app->userData = &appState;
        app->onAppCmd = app_handle_cmd;

        std::shared_ptr<Options> options = std::make_shared<Options>();
        if (!UpdateOptionsFromSystemProperties(*options)) {
            return;
        }

        std::shared_ptr<PlatformData> data = std::make_shared<PlatformData>();
        data->applicationVM = app->activity->vm;
        data->applicationActivity = app->activity->clazz;

        bool requestRestart = false;
        bool exitRenderLoop = false;

        // Create platform-specific implementation.
        std::shared_ptr<IPlatformPlugin> platformPlugin = CreatePlatformPlugin(options, data);
        // Create graphics API implementation.
        std::shared_ptr<IGraphicsPlugin> graphicsPlugin = CreateGraphicsPlugin(options, platformPlugin);

        // Initialize the OpenXR program.
        std::shared_ptr<IOpenXrProgram> program = CreateOpenXrProgram(options, platformPlugin, graphicsPlugin);

        // Initialize the loader for this platform
        PFN_xrInitializeLoaderKHR initializeLoader = nullptr;
        if (XR_SUCCEEDED(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)(&initializeLoader)))) {
            XrLoaderInitInfoAndroidKHR loaderInitInfoAndroid;
            memset(&loaderInitInfoAndroid, 0, sizeof(loaderInitInfoAndroid));
            loaderInitInfoAndroid.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
            loaderInitInfoAndroid.next = NULL;
            loaderInitInfoAndroid.applicationVM = app->activity->vm;
            loaderInitInfoAndroid.applicationContext = app->activity->clazz;
            initializeLoader((const XrLoaderInitInfoBaseHeaderKHR*)&loaderInitInfoAndroid);
        }
        program->CreateInstance();
        program->InitializeSystem();
        program->InitializeSession();
        program->CreateSwapchains();

        gVideoGLTex=new VideoGLTex();
        gCPPEnv->CallVoidMethod(gVideoSurfaceJObj,gNewSurfaceAndTexMID,gVideoGLTex->mGlTexture);

        program->StartPlayer(gNativeWindow);

        while (app->destroyRequested == 0){
            for (;;) {
                int events;
                struct android_poll_source* source;
                // If the timeout is zero, returns immediately without blocking.
                // If the timeout is negative, waits indefinitely until an event appears.
                const int timeoutMilliseconds = (!appState.Resumed && !program->IsSessionRunning() && app->destroyRequested == 0) ? -1 : 0;
                if (ALooper_pollAll(timeoutMilliseconds, nullptr, &events, (void**)&source) < 0) {
                    break;
                }

                // Process this event.
                if (source != nullptr) {
                    source->process(app, source);
                }
            }

            program->PollEvents(&exitRenderLoop, &requestRestart);

            if (exitRenderLoop && !requestRestart) {
                ANativeActivity_finish(app->activity);
            }

            if (!program->IsSessionRunning()) {
                // Throttle loop since xrWaitFrame won't be called.
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                continue;
            }

            program->PollActions();
            program->RenderFrame();
        }
        app->activity->vm->DetachCurrentThread();
    }
    catch (const std::exception &ex)
    {
        Log::Write(Log::Level::Error, ex.what());
    }
    catch (...)
    {
        Log::Write(Log::Level::Error, "Unknown Error");
    }
}


}