#ifndef OPENXR_VIDEOPLAYER_DEMO_VIDEOGLTEX_H
#define OPENXR_VIDEOPLAYER_DEMO_VIDEOGLTEX_H

#include "common/gfxwrapper_opengl.h"

class VideoGLTex{
public:
    GLuint mGlTexture=0;

    VideoGLTex();
    ~VideoGLTex();
};


#endif
