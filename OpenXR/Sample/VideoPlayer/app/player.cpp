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

CPlayer::CPlayer() : mExtractor(nullptr), mVideoCodec(nullptr), mFd(-1), mStarted(false) {
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
                media_status_t status = AMediaCodec_configure(videoCodec, format, nullptr, nullptr, 0);
                if (status != AMEDIA_OK) {
                    Log::Write(Log::Level::Error, Fmt("AMediaCodec_configure error, status = %d", status));
                } else {
                    Log::Write(Log::Level::Info, Fmt("video AMediaCodec_configure successfuly"));
                }
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

        uint64_t pts_offset = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            int32_t index = AMediaExtractor_getSampleTrackIndex(mExtractor);
            int64_t pts = AMediaExtractor_getSampleTime(mExtractor);
            pts += pts_offset;
            if (index < 0) {
                // Play from the beginning when reach end of the file
                Log::Write(Log::Level::Info, Fmt("the video file is end, index:%d", index));
                AMediaExtractor_seekTo(mExtractor, 0, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);
                pts_offset += videoDurationUs;
            } else if (index == audioTrackIndex) {
                //audio
                ssize_t bufferIdx_a = AMediaCodec_dequeueInputBuffer(audioCodec, 1);
                if (bufferIdx_a >= 0) {
                    size_t bufferSize = 0;
                    uint8_t *buffer = AMediaCodec_getInputBuffer(audioCodec, bufferIdx_a, &bufferSize);
                    ssize_t size = AMediaExtractor_readSampleData(mExtractor, buffer, bufferSize);
                    if (size > 0) {
                        AMediaCodec_queueInputBuffer(audioCodec, bufferIdx_a, 0, size, pts, 0);
                    }
                    AMediaExtractor_advance(mExtractor);
                } else {
                    Log::Write(Log::Level::Info, Fmt("audio AMediaCodec_dequeueInputBuffer bufferIdx_a:%d", bufferIdx_a));
                }
            } else if (index == videoTrackIndex) {
                //video
                ssize_t bufferIdx = AMediaCodec_dequeueInputBuffer(videoCodec, 1);
                if (bufferIdx >= 0) {
                    size_t bufferSize = 0;
                    uint8_t *buffer = AMediaCodec_getInputBuffer(videoCodec, bufferIdx, &bufferSize);
                    ssize_t size = AMediaExtractor_readSampleData(mExtractor, buffer, bufferSize);
                    AMediaCodec_queueInputBuffer(videoCodec, bufferIdx, 0, size, pts, 0);
                    AMediaExtractor_advance(mExtractor);
                } else {
                    Log::Write(Log::Level::Info, Fmt("video AMediaCodec_dequeueInputBuffer bufferIdx:%d", bufferIdx));
                }
            }

            //audio output buffer
            if (audioCodec) {
                AMediaCodecBufferInfo outputBufferInfo_a;
                ssize_t bufferIdx_a = AMediaCodec_dequeueOutputBuffer(audioCodec, &outputBufferInfo_a, 1);
                if (bufferIdx_a >= 0) {
                    uint8_t *outputBuffer = AMediaCodec_getOutputBuffer(audioCodec, bufferIdx_a, nullptr);
                    size_t outputDataSize = outputBufferInfo_a.size;
                    const uint32_t numSamples = outputDataSize / (audioChannelCount * sizeof(int16_t));
                    const int64_t timeout = int64_t(numSamples * 1.0 * oboe::kNanosPerMillisecond / audioSampleRate);
                    oboe::ResultWithValue<int32_t> ret = audioStreamPlay->write(outputBuffer, numSamples, timeout);
                    if (ret.value() == numSamples) {
                        AMediaCodec_releaseOutputBuffer(audioCodec, bufferIdx_a, true);
                    } else {
                        Log::Write(Log::Level::Error, Fmt("audio write ret:%d", ret));
                        AMediaCodec_releaseOutputBuffer(audioCodec, bufferIdx_a, true);
                    }
                }
            }
            
            //video output buffer
            if (videoCodec) {
                AMediaCodecBufferInfo outputBufferInfo;
                ssize_t bufferIdx = AMediaCodec_dequeueOutputBuffer(videoCodec, &outputBufferInfo, 1);
                if (bufferIdx >= 0) {
                    if (outputBufferInfo.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
                        Log::Write(Log::Level::Error, Fmt("video codec end"));
                    }
                    uint8_t *outputBuffer = AMediaCodec_getOutputBuffer(videoCodec, bufferIdx, nullptr);
                    if (outputBuffer) {
                        std::shared_ptr<MediaFrame> frame = std::make_shared<MediaFrame>();
                        frame->type = mediaTypeVideo;
                        frame->width = videoWidth;
                        frame->height = videoHeight;
                        frame->pts = outputBufferInfo.presentationTimeUs / 1000;
                        frame->number = 0;
                        frame->data = outputBuffer + outputBufferInfo.offset;
                        frame->size = outputBufferInfo.size;
                        frame->bufferIndex = bufferIdx;
                        
                        mMediaListMutex.lock();
                        mMediaList.push_back(frame);
                        //Log::Write(Log::Level::Error, Fmt("mMediaList size:%d", mMediaList.size()));
                        mMediaListMutex.unlock();
                    }
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
    std::lock_guard<std::mutex> guard(mMediaListMutex);
    if (mMediaList.size()) {
        return mMediaList.front();
    } else {
        return nullptr;
    }
}

bool CPlayer::releaseFrame(std::shared_ptr<MediaFrame> &frame) {
    if (frame.get() == nullptr) {
        return true;
    }
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();  //in millisecond
    if (now < frame->pts) {
        return false;
    }
    std::lock_guard<std::mutex> guard(mMediaListMutex);
    if (mMediaList.size() <= 1) {
        return true;
    }
    auto &it = mMediaList.front();
    if (it.get() && it == frame) {
        AMediaCodec_releaseOutputBuffer(this->mVideoCodec, it->bufferIndex, true);
        mMediaList.pop_front();
    }
    return true;
}

void CPlayer::getAlignment(int32_t &width, int32_t &height, int32_t alignment) {
    width = (width + alignment - 1) / alignment * alignment;
    height = (height + alignment - 1) / alignment * alignment;
}
