// Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved.
//
// This file provides an example of 3D decoding and play.
//
// Created by shunxiang at 2022/09/22

#include "player.h"
#include "pch.h"
#include "common.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <chrono>

#define TAG "NativeCodec"
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

int64_t systemnanotime()
{
	timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return now.tv_sec * 1000000000LL + now.tv_nsec;
}

CPlayer::CPlayer() : mExtractor(nullptr), mVideoCodec(nullptr), mFd(-1), mStarted(false) {
    this->mMediaFrame=std::make_shared<MediaFrame>();
}

CPlayer::~CPlayer() {
    if (mExtractor) {
        AMediaExtractor_delete(mExtractor);
        mExtractor = nullptr;
    }
    if (mFd > 0) {
        close(mFd);
        mFd = -1;
    }
}

bool CPlayer::setDataSource(const char* source, int32_t& videoWidth, int32_t& videoHeight) {
    if (mExtractor == nullptr) {
        mExtractor = AMediaExtractor_new();
        if (mExtractor == nullptr) {
            Log::Write(Log::Level::Error, "AMediaExtractor_new error");
            return false;
        }
    }

    struct stat statbuff;
    int32_t fileLen = -1;
    if (stat(source, &statbuff) < 0) {
        Log::Write(Log::Level::Error, Fmt("setDataSource error, open file %s error", source));
        return false;  
    } else {  
        fileLen = statbuff.st_size;  
    } 

    if (mFd) {
        close(mFd);
        mFd = -1;
    }
    mFd = open(source, O_RDONLY);
    if (mFd < 0) {
        Log::Write(Log::Level::Error, Fmt("setDataSource error, open file %s error, ret=%d", source, mFd));
        return false;
    }

    media_status_t status = AMediaExtractor_setDataSourceFd(mExtractor, mFd, 0, fileLen);
    if (status != AMEDIA_OK) {
        Log::Write(Log::Level::Error, Fmt("setDataSource error, ret = %d", status));
        return false;
    }

    size_t track = AMediaExtractor_getTrackCount(mExtractor);
    Log::Write(Log::Level::Error, Fmt("setDataSource success, file size %d track = %d", fileLen, track));
    for (auto i = 0; i < track; i++) {
        const char *mime = nullptr;
        AMediaFormat *format = AMediaExtractor_getTrackFormat(mExtractor, i);
        AMediaFormat_getString(format, "mime", &mime);
        if (strstr(mime, "video")) {
            AMediaFormat_getInt32(format, "width", &videoWidth);
            AMediaFormat_getInt32(format, "height", &videoHeight);
            getAlignment(videoWidth, videoHeight, mAlignment);
            Log::Write(Log::Level::Error, Fmt("setDataSource video width:%d height:%d", videoWidth, videoHeight));
        }
    }
    return true;
}

bool CPlayer::start() {
    if (mExtractor == nullptr) {
        return false;
    }
    if (mStarted) {
        return true;
    }

    std::thread ([=](){
        int32_t videoTrackIndex = -1;
        int32_t audioTrackIndex = -1;
        AMediaCodec *videoCodec = nullptr;
        AMediaCodec *audioCodec = nullptr;
        int32_t audioChannelCount = 0;
        int32_t audioSampleRate = 0;
        int32_t videoWidth = 0;
        int32_t videoHeight = 0;
        int64_t videoDurationUs = 0;

        int count = 0;
        std::shared_ptr<oboe::AudioStream> audioStreamPlay;

        size_t track = AMediaExtractor_getTrackCount(mExtractor);
        for (auto i = 0; i < track; i++) {
            const char *mime = nullptr;
            AMediaFormat *format = AMediaExtractor_getTrackFormat(mExtractor, i);
            Log::Write(Log::Level::Error, Fmt("track %d format %s", i, AMediaFormat_toString(format)));
            AMediaFormat_getString(format, "mime", &mime);
            if (strstr(mime, "video")) {
                videoTrackIndex = i;
                AMediaFormat_getInt32(format, "width", &videoWidth);
                AMediaFormat_getInt32(format, "height", &videoHeight);
                AMediaFormat_getInt64(format, "durationUs", &videoDurationUs);
                getAlignment(videoWidth, videoHeight, mAlignment);
                videoCodec = AMediaCodec_createDecoderByType(mime);
                if (videoCodec == nullptr) {
                    Log::Write(Log::Level::Error, Fmt("create mediacodec %s error", mime));
                }
                this->mVideoCodec = videoCodec;

                media_status_t status;
                if(this->mNativeWindow)
                {
                    Log::Write(Log::Level::Error, Fmt("AMediaCodec_configure this->mNativeWindow"));
                    status = AMediaCodec_configure(videoCodec, format, this->mNativeWindow, nullptr,0);
                }
                else
                    status=AMediaCodec_configure(videoCodec,format,nullptr,nullptr,0);

                if (status != AMEDIA_OK) 
                    Log::Write(Log::Level::Error, Fmt("Error AMediaCodec_configure error, status = %d", status));
            } else if (strstr(mime, "audio")) {
                audioTrackIndex = i;
                AMediaFormat_getInt32(format, "channel-count", &audioChannelCount);
                AMediaFormat_getInt32(format, "sample-rate", &audioSampleRate);
                audioCodec = AMediaCodec_createDecoderByType(mime);
                if (audioCodec == nullptr) {
                    Log::Write(Log::Level::Error, Fmt("create mediacodec %s error", mime));
                }
                media_status_t status = AMediaCodec_configure(audioCodec, format, nullptr, nullptr, 0);
                if (status != AMEDIA_OK) {
                    Log::Write(Log::Level::Error, Fmt("AMediaCodec_configure error, status = %d", status));
                } else {
                    Log::Write(Log::Level::Info, Fmt("audio AMediaCodec_configure successfuly"));
                }

                //init audio output
                oboe::AudioStreamBuilder playStreamBuilder;
                playStreamBuilder.setDirection(oboe::Direction::Output);
                playStreamBuilder.setPerformanceMode(oboe::PerformanceMode::None);
                playStreamBuilder.setSharingMode(oboe::SharingMode::Exclusive);
                playStreamBuilder.setFormat(oboe::AudioFormat::I16);
                playStreamBuilder.setChannelCount(oboe::ChannelCount(audioChannelCount));
                playStreamBuilder.setSampleRate(audioSampleRate);

                oboe::Result ret = playStreamBuilder.openStream(audioStreamPlay);
                if (ret != oboe::Result::OK) {
                    Log::Write(Log::Level::Error, Fmt("Failed to open playback stream. Error: %s", oboe::convertToText(ret)));
                    return false;
                }
                int32_t bufferSizeFrames = audioStreamPlay->getFramesPerBurst() * 2;
                ret = audioStreamPlay->setBufferSizeInFrames(bufferSizeFrames);
                Log::Write(Log::Level::Error, Fmt("bufferSizeFrames: %d", bufferSizeFrames));
                if (ret != oboe::Result::OK) {
                    Log::Write(Log::Level::Error, Fmt("Failed to set playback stream buffer size to: %d. Error: %s", bufferSizeFrames, oboe::convertToText(ret)));
                    return false;
                }
                ret = audioStreamPlay->start();
                if (ret != oboe::Result::OK) {
                    Log::Write(Log::Level::Error, Fmt("Failed to start playback stream. Error: %s", oboe::convertToText(ret)));
                    return false;
                }
            }
            AMediaFormat_delete(format);
        }

        if (videoCodec) {
            media_status_t status = AMediaCodec_start(videoCodec);
            if (status != AMEDIA_OK) {
                Log::Write(Log::Level::Error, Fmt("AMediaCodec_start error, status = %d", status));
            } else {
                Log::Write(Log::Level::Info, "video AMediaCodec_start successfully");
            }
            status = AMediaExtractor_selectTrack(mExtractor, videoTrackIndex);
            if (status != AMEDIA_OK) {
                Log::Write(Log::Level::Error, Fmt("video AMediaExtractor_selectTrack error, status = %d", status));
                return false;
            }
        }
        if (audioCodec) {
            media_status_t status = AMediaCodec_start(audioCodec);
            if (status != AMEDIA_OK) {
                Log::Write(Log::Level::Error, Fmt("AMediaCodec_start error, status = %d", status));
            } else {
                Log::Write(Log::Level::Info, "audio AMediaCodec_start successfully");
            }
            status = AMediaExtractor_selectTrack(mExtractor, audioTrackIndex);
            if (status != AMEDIA_OK) {
                Log::Write(Log::Level::Error, Fmt("audio AMediaExtractor_selectTrack error, status = %d", status));
                return false;
            }          
        }

        bool sawInputEOS=false,sawOutputEOS= false;
        while(true)
        {
            if(!sawInputEOS)
            {
                ssize_t bufidx=AMediaCodec_dequeueInputBuffer(videoCodec,2000);
                if(bufidx>=0)
                {
                    size_t bufsize=0;
                    uint8_t* buf=AMediaCodec_getInputBuffer(videoCodec,bufidx,&bufsize);
                    ssize_t sampleSize=AMediaExtractor_readSampleData(mExtractor,buf,bufsize);
                    if (sampleSize < 0)
                    {
                        sampleSize = 0;
                        sawInputEOS = true;
                    }
					int64_t presentationTimeUs=AMediaExtractor_getSampleTime(mExtractor);

                    AMediaCodec_queueInputBuffer(videoCodec,bufidx,0,sampleSize,presentationTimeUs,0);
                    AMediaExtractor_advance(mExtractor);
                }
            }

            if (!sawOutputEOS)
            {
                AMediaCodecBufferInfo info;
                ssize_t status=AMediaCodec_dequeueOutputBuffer(videoCodec,&info,0);
                if(status>=0)
                {
                    if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)
                    {
                        sawOutputEOS = true;
                    }
					int64_t presentationNano=info.presentationTimeUs*1000;
					if(this->mRenderstart<0) {
                        this->mRenderstart = systemnanotime() - presentationNano;
                    }

					int64_t delay=this->mRenderstart+presentationNano-systemnanotime();
					if(delay>0)
					{
						usleep(delay/1000);
					}

                    AMediaCodec_releaseOutputBuffer(videoCodec,status,info.size!=0);
                }
            }
        }
        Log::Write(Log::Level::Error, Fmt("exit thread......"));
    }).detach();

    return true;
}

bool CPlayer::stop() {
    return true;
}

std::shared_ptr<MediaFrame> CPlayer::getFrame() {
   return this->mMediaFrame;
}

void CPlayer::getAlignment(int32_t &width, int32_t &height, int32_t alignment) {
    width = (width + alignment - 1) / alignment * alignment;
    height = (height + alignment - 1) / alignment * alignment;
}
