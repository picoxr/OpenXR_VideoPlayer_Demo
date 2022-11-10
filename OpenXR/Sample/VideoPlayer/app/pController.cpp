//
// Created by Alex on 2021/1/29.
//

#include "pController.h"

XrInstance mControllerInstance;
PFN_xrGetControllerConnectionStatePico pfnXrGetControllerConnectionStatePico = nullptr;
PFN_xrSetEngineVersionPico pfnXrSetEngineVersionPico = nullptr;
PFN_xrSetControllerEventCallbackPico pfnXrSetControllerEventCallbackPico = nullptr;
PFN_xrResetControllerSensorPico pfnXrResetControllerSensorPico = nullptr;
PFN_xrGetConnectDeviceMacPico pfnXrGetConnectDeviceMacPico = nullptr;
PFN_xrStartCVControllerThreadPico pfnXrStartCVControllerThreadPico = nullptr;
PFN_xrStopCVControllerThreadPico pfnXrStopCVControllerThreadPico = nullptr;
PFN_xrGetControllerAngularVelocityStatePico pfnXrGetControllerAngularVelocityStatePico = nullptr;
PFN_xrGetControllerAccelerationStatePico pfnXrGetControllerAccelerationStatePico = nullptr;
PFN_xrSetMainControllerHandlePico pfnXrSetMainControllerHandlePico = nullptr;
PFN_xrGetMainControllerHandlePico pfnXrGetMainControllerHandlePico = nullptr;
PFN_xrResetHeadSensorForControllerPico pfnXrResetHeadSensorForControllerPico = nullptr;
PFN_xrSetIsEnbleHomeKeyPico pfnXrSetIsEnbleHomeKeyPico = nullptr;
PFN_xrGetHeadSensorDataPico pfnXrGetHeadSensorDataPico = nullptr;
PFN_xrGetControllerSensorDataPredictPico pfnXrGetControllerSensorDataPredictPico = nullptr;
PFN_xrVibrateControllerPico pfnXrVibrateControllerPico = nullptr;
PFN_xrGetControllerLinearVelocityStatePico pfnXrGetControllerLinearVelocityStatePico = nullptr;
PFN_xrGetControllerSensorDataPico pfnXrGetControllerSensorDataPico = nullptr;
PFN_xrGetControllerFixedSensorStatePico pfnXrGetControllerFixedSensorStatePico = nullptr;
PFN_xrGetControllerTouchValuePico pfnXrGetControllerTouchValuePico = nullptr;
PFN_xrGetControllerGripValuePico pfnXrGetControllerGripValuePico = nullptr;

namespace pxr {

    void InitializeGraphicDeivce(XrInstance mInstance) {
        mControllerInstance = mInstance;
        xrGetInstanceProcAddr(mInstance, "xrGetControllerConnectionStatePico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrGetControllerConnectionStatePico));
        xrGetInstanceProcAddr(mInstance, "xrSetEngineVersionPico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrSetEngineVersionPico));
        xrGetInstanceProcAddr(mInstance, "xrSetControllerEventCallbackPico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrSetControllerEventCallbackPico));
        xrGetInstanceProcAddr(mInstance, "xrResetControllerSensorPico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrResetControllerSensorPico));
        xrGetInstanceProcAddr(mInstance, "xrGetConnectDeviceMacPico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrGetConnectDeviceMacPico));
        xrGetInstanceProcAddr(mInstance, "xrStartCVControllerThreadPico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrStartCVControllerThreadPico));
        xrGetInstanceProcAddr(mInstance, "xrStopCVControllerThreadPico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrStopCVControllerThreadPico));
        xrGetInstanceProcAddr(mInstance, "xrGetControllerAngularVelocityStatePico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrGetControllerAngularVelocityStatePico));
        xrGetInstanceProcAddr(mInstance, "xrGetControllerAccelerationStatePico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrGetControllerAccelerationStatePico));
        xrGetInstanceProcAddr(mInstance, "xrSetMainControllerHandlePico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrSetMainControllerHandlePico));
        xrGetInstanceProcAddr(mInstance, "xrGetMainControllerHandlePico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrGetMainControllerHandlePico));
        xrGetInstanceProcAddr(mInstance, "xrResetHeadSensorForControllerPico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrResetHeadSensorForControllerPico));
        xrGetInstanceProcAddr(mInstance, "xrSetIsEnbleHomeKeyPico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrSetIsEnbleHomeKeyPico));
        xrGetInstanceProcAddr(mInstance, "xrGetHeadSensorDataPico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrGetHeadSensorDataPico));
        xrGetInstanceProcAddr(mInstance, "xrGetControllerSensorDataPredictPico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrGetControllerSensorDataPredictPico));
        xrGetInstanceProcAddr(mInstance, "xrVibrateControllerPico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrVibrateControllerPico));
        xrGetInstanceProcAddr(mInstance, "xrGetControllerLinearVelocityStatePico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrGetControllerLinearVelocityStatePico));
        xrGetInstanceProcAddr(mInstance, "xrGetControllerSensorDataPico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrGetControllerSensorDataPico));
        xrGetInstanceProcAddr(mInstance, "xrGetControllerFixedSensorStatePico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrGetControllerFixedSensorStatePico));
        xrGetInstanceProcAddr(mInstance, "xrGetControllerGripValuePico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrGetControllerGripValuePico));
        xrGetInstanceProcAddr(mInstance, "xrGetControllerTouchValuePico",
                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrGetControllerTouchValuePico));
    }

    int Pxr_GetControllerConnectionState(
            uint8_t controllerhandle, uint8_t *status) {
        if (pfnXrGetControllerConnectionStatePico != nullptr) {
            return pfnXrGetControllerConnectionStatePico(mControllerInstance,controllerhandle, status);
        } else {
            return -1;
        }
    }

    int Pxr_SetEngineVersion(const char *version) {
        if (pfnXrSetEngineVersionPico != nullptr) {
            return pfnXrSetEngineVersionPico(mControllerInstance,version);
        } else {
            return -1;
        }
    }

    int Pxr_SetControllerEventCallback(bool enable_controller_callback) {
        if (pfnXrSetControllerEventCallbackPico != nullptr) {
            return pfnXrSetControllerEventCallbackPico(mControllerInstance,enable_controller_callback);
        } else {
            return -1;
        }
    }

    int Pxr_ResetControllerSensor(int controllerHandle) {
        if (pfnXrResetControllerSensorPico != nullptr) {
            return pfnXrResetControllerSensorPico(mControllerInstance,controllerHandle);
        } else {
            return -1;
        }
    }

    int Pxr_GetConnectDeviceMac(char *mac) {
        if (pfnXrGetConnectDeviceMacPico != nullptr) {
            return pfnXrGetConnectDeviceMacPico(mControllerInstance,mac);
        } else {
            return -1;
        }
    }

    int Pxr_StartCVControllerThread(int headSensorState, int handSensorState) {
        if (pfnXrStartCVControllerThreadPico != nullptr) {
            return pfnXrStartCVControllerThreadPico(mControllerInstance,headSensorState, handSensorState);
        } else {
            return -1;
        }
    }

    int Pxr_StopCVControllerThread(int headSensorState, int handSensorState) {
        if (pfnXrStopCVControllerThreadPico != nullptr) {
            return pfnXrStopCVControllerThreadPico(mControllerInstance,headSensorState, handSensorState);
        } else {
            return -1;
        }
    }

    int Pxr_GetControllerAngularVelocityState(int controllerHandle, float *data) {
        if (pfnXrGetControllerAngularVelocityStatePico != nullptr) {
            return pfnXrGetControllerAngularVelocityStatePico(mControllerInstance,controllerHandle, data);
        } else {
            return -1;
        }
    }

    int Pxr_GetControllerAccelerationState(int controllerHandle, float *data) {
        if (pfnXrGetControllerAccelerationStatePico != nullptr) {
            return pfnXrGetControllerAccelerationStatePico(mControllerInstance,controllerHandle, data);
        } else {
            return -1;
        }
    }

    int Pxr_SetMainControllerHandle(int controllerHandle) {
        if (pfnXrSetMainControllerHandlePico != nullptr) {
            return pfnXrSetMainControllerHandlePico(mControllerInstance,controllerHandle);
        } else {
            return -1;
        }
    }

    int Pxr_GetMainControllerHandle(int *controllerHandle) {
        if (pfnXrGetMainControllerHandlePico != nullptr) {
            return pfnXrGetMainControllerHandlePico(mControllerInstance,controllerHandle);
        } else {
            return -1;
        }
    }

    int Pxr_ResetHeadSensorForController() {
        if (pfnXrResetHeadSensorForControllerPico != nullptr) {
            return pfnXrResetHeadSensorForControllerPico(mControllerInstance);
        } else {
            return -1;
        }
    }

    int Pxr_SetIsEnableHomeKey(bool isEnable) {
        if (pfnXrSetIsEnbleHomeKeyPico != nullptr) {
            return pfnXrSetIsEnbleHomeKeyPico(mControllerInstance,isEnable);
        } else {
            return -1;
        }
    }

    int Pxr_GetHeadSensorData(float *data) {
        if (pfnXrGetHeadSensorDataPico != nullptr) {
            return pfnXrGetHeadSensorDataPico(mControllerInstance,data);
        } else {
            return -1;
        }
    }

    int Pxr_GetControllerSensorDataPredict(int controllerHandle, float headSensorData[],
                                           float predictTime, float *data) {
        if (pfnXrGetControllerSensorDataPredictPico != nullptr) {
            return pfnXrGetControllerSensorDataPredictPico(mControllerInstance,controllerHandle, headSensorData,
                                                                predictTime, data);
        } else {
            return -1;
        }
    }

    int Pxr_VibrateController(float strength, int time, int controllerHandle) {
        if (pfnXrVibrateControllerPico != nullptr) {
            return pfnXrVibrateControllerPico(mControllerInstance,strength, time, controllerHandle);
        } else {
            return -1;
        }
    }

    int Pxr_GetControllerLinearVelocityState(int controllerHandle, float *data) {
        if (pfnXrGetControllerLinearVelocityStatePico != nullptr) {
            return pfnXrGetControllerLinearVelocityStatePico(mControllerInstance,controllerHandle, data);
        } else {
            return -1;
        }
    }

    int Pxr_GetControllerSensorData(int controllerHandle, float headSensorData[], float *data) {
        if (pfnXrGetControllerSensorDataPico != nullptr) {
            return pfnXrGetControllerSensorDataPico(mControllerInstance,controllerHandle, headSensorData, data);
        } else {
            return -1;
        }
    }

    int Pxr_GetControllerFixedSensorState(int controllerHandle, float *data) {
        if (pfnXrGetControllerFixedSensorStatePico != nullptr) {
            return pfnXrGetControllerFixedSensorStatePico(mControllerInstance,controllerHandle, data);
        } else {
            return -1;
        }
    }


    int Pxr_GetControllerGripValue(int controllerSerialNum, int *tripvalue) {
        if (pfnXrGetControllerGripValuePico != nullptr) {
            return pfnXrGetControllerGripValuePico(mControllerInstance,controllerSerialNum, tripvalue);
        } else {
            return -1;
        }
    }

    int Pxr_GetControllerTouchValue(int controllerSerialNum, int length, int *value) {
        if (pfnXrGetControllerTouchValuePico != nullptr) {
            return pfnXrGetControllerTouchValuePico(mControllerInstance,controllerSerialNum, length, value);
        } else {
            return -1;
        }
    }


}