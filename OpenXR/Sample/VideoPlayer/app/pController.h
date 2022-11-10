//
// Created by Alex on 2021/1/29.
//

#ifndef HELLOXR_PCONTROLLER_H
#define HELLOXR_PCONTROLLER_H

#endif //HELLOXR_PCONTROLLER_H

#include "pch.h"

namespace pxr {

    void InitializeGraphicDeivce(XrInstance mInstance);
//pico controller

    int Pxr_GetControllerConnectionState(
            uint8_t controllerhandle, uint8_t *status);

    int Pxr_SetEngineVersion(const char *version);

    int Pxr_SetControllerEventCallback(bool enable_controller_callback);

    int Pxr_ResetControllerSensor(int controllerHandle);

    int Pxr_GetConnectDeviceMac(char *mac);

    int Pxr_StartCVControllerThread(int headSensorState, int handSensorState);

    int Pxr_StopCVControllerThread(int headSensorState, int handSensorState);

    int Pxr_GetControllerAngularVelocityState(int controllerHandle, float *data);

    int Pxr_GetControllerAccelerationState(int controllerHandle, float *data);

    int Pxr_SetMainControllerHandle(int controllerHandle);

    int Pxr_GetMainControllerHandle(int *controllerHandle);

    int Pxr_ResetHeadSensorForController();

    int Pxr_SetIsEnableHomeKey(bool isEnable);

    int Pxr_GetHeadSensorData(float *data);

    int Pxr_GetControllerSensorDataPredict(int controllerHandle, float headSensorData[],
                                           float predictTime, float *data);

    int Pxr_VibrateController(float strength, int time, int controllerHandle);

    int Pxr_GetControllerLinearVelocityState(int controllerHandle, float *data);

    int Pxr_GetControllerSensorData(int controllerHandle, float headSensorData[], float *data);

    int Pxr_GetControllerFixedSensorState(int controllerHandle, float *data);

    int Pxr_GetControllerGripValue(int controllerSerialNum, int *gripvalue);

    int Pxr_GetControllerTouchValue(int controllerSerialNum, int length, int *value);
}