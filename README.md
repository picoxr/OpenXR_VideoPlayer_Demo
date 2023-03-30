## OpenXR_VideoPlayer_Demo
  - If you have any questions/comments, please visit [**Pico Developer Support Portal**](https://picodevsupport.freshdesk.com/support/home) and raise your question there.

## OpenXR SDK Version
  - v2.1.0
  
## Description
  - This demo shows how to implement 360, 3D-SBS, 2D VR video player with Pico OpenXR SDK use OpenGLES and Vulkan.

## Usage
  - Scene： 3D-side-by-side(3D-SBS)
    Demo of VR video player for 3D side-by-side (left-right) video format.

  - Scene： 360 
    Demo of VR video player for 360 video format.

  - Scene： 2D
    Demo of VR video player for 2D video format.

### How select video mode and Specify video file name
  In the `cpp/app/options.h` file `VideoMode` field indicates videomode and `VideoFileName` indicates the video file used to playback. GraphicsPlugin filed indicates what rendering API to use, you can specify `OpenGLES` or `Vulkan2`.
