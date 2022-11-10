// Copyright (c) 2017-2022 PICO Inc, All rights reserved.
//
// This file provides an example of 3D decoding and play.
//
// Created by shunxiang at 2022/09/22

#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <list>
#include <memory>
#include <media/NdkMediaExtractor.h>
#include "oboe/Oboe.h"

typedef enum {
    mediaTypeVideo = 0,
    mediaTypeAudio
}mediaType;

typedef struct MediaFrame_tag {
    MediaFrame_tag() : type(mediaTypeVideo), pts(0), number(0), data(nullptr), size(0) {};
    mediaType type;
    uint64_t pts;
    int32_t width;
    int32_t height;
    uint32_t number;
    uint8_t* data;
    uint32_t size;
    ssize_t bufferIndex;
}MediaFrame;

class CPlayer {

public:
    CPlayer();

    ~CPlayer();

    bool setDataSource(const char* source);

    bool start();

    bool stop();

    std::shared_ptr<MediaFrame> getFrame();

    bool releaseFrame(std::shared_ptr<MediaFrame> &frame);

public:
    AMediaExtractor* mExtractor;
    AMediaCodec*     mVideoCodec;
    int32_t          mFd;
    bool             mStarted;

    std::mutex       mMediaListMutex;
    std::list<std::shared_ptr<MediaFrame>> mMediaList;

};