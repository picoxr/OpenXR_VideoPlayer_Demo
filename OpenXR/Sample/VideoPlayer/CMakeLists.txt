# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

# build native_app_glue as a static lib
set(APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
include_directories(${APP_GLUE_DIR})
add_library( app-glue STATIC ${APP_GLUE_DIR}/android_native_app_glue.c)


add_definitions(-DXR_USE_PLATFORM_ANDROID)
add_definitions(-DXR_USE_GRAPHICS_API_OPENGL_ES)
add_definitions(-DXR_USE_GRAPHICS_API_VULKAN)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")


file(GLOB LOCAL_HEADERS  "app/*.h")
file(GLOB LOCAL_SOURCE   "app/*.cpp")
file(GLOB VULKAN_SHADERS "app/vulkan_shaders/*.glsl")

LINK_DIRECTORIES(openxr_loader/${ANDROID_ABI})
include_directories(openxr_loader/include)
LINK_DIRECTORIES(build/Oboe/prefab/modules/oboe/libs/android.${ANDROID_ABI})
include_directories(build/Oboe/prefab/modules/oboe/include)

add_library( # Sets the name of the library.
        player
		SHARED
        ${LOCAL_SOURCE}
        ${LOCAL_HEADERS}
		${VULKAN_SHADERS}
        openxr_loader/include/common/gfxwrapper_opengl.c
		)

source_group("Headers" FILES ${LOCAL_HEADERS})
source_group("Shaders" FILES ${VULKAN_SHADERS})


if(VulkanHeaders_INCLUDE_DIRS)
    target_include_directories(player
            PRIVATE
            ${VulkanHeaders_INCLUDE_DIRS}
            )
endif()

if(Vulkan_LIBRARY)
    target_link_libraries(player ${Vulkan_LIBRARY})
endif()

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
                       player
		               oboe
                       openxr_loader
                       android
		               mediandk
                       vulkan
                       EGL
                       GLESv3
                       app-glue
                       log
                       )