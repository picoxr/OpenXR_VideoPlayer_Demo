// Copyright (c) 2017-2022 The Khronos Group Inc
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

struct Options {
    std::string GraphicsPlugin{"Vulkan2"};     //configurable: Vulkan2, OpenGLES

    std::string FormFactor{"Hmd"};

    std::string ViewConfiguration{"Stereo"};

    std::string EnvironmentBlendMode{"Opaque"};

    std::string AppSpace{"Local"};

    std::string VideoMode{"3D-SBS"};              //Configurable video mode: 2D, 3D-SBS, 360

    std::string VideoFileName{"/sdcard/test3d.mp4"};

    struct {
        XrFormFactor FormFactor{XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY};

        XrViewConfigurationType ViewConfigType{XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};

        XrEnvironmentBlendMode EnvironmentBlendMode{XR_ENVIRONMENT_BLEND_MODE_OPAQUE};
    } Parsed;
};
