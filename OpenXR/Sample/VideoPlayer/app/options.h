// Copyright (c) 2017-2020 The Khronos Group Inc
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

struct Options {
    std::string GraphicsPlugin;

    std::string FormFactor{"Hmd"};

    std::string ViewConfiguration{"Stereo"};

    std::string EnvironmentBlendMode{"Opaque"};

    std::string AppSpace{"Local"};

    std::string VideoMode{"3D-SBS"};  //Configurable video mode: 3D-SBS, 360

    std::string VideoFileName{"/sdcard/test3d.mp4"};
};
