#include "VideoGLTex.h"
#include "pch.h"
#include "common.h"

VideoGLTex::VideoGLTex()
{
    GLint currentTexture=0;
    glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES,&currentTexture);

    glGenTextures(1,&this->mGlTexture);



    glBindTexture(GL_TEXTURE_EXTERNAL_OES,this->mGlTexture);
    glTexParameterf(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_EXTERNAL_OES,currentTexture);




}

VideoGLTex::~VideoGLTex()
{

}

