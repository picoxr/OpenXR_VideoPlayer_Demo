package com.khronos.player;

import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.view.SurfaceView;

import java.io.FileOutputStream;
import java.io.InputStream;

public class MainActivity extends android.app.NativeActivity
{
    static
    {
        System.loadLibrary("openxr_loader");
        System.loadLibrary("player");
    }
    private static final int REQUEST_EXTERNAL_STORAGE = 1;
    private static String[] PERMISSIONS_STORAGE = {
          "android.permission.READ_EXTERNAL_STORAGE",
          "android.permission.WRITE_EXTERNAL_STORAGE"
    };
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        requestPermissions(PERMISSIONS_STORAGE,REQUEST_EXTERNAL_STORAGE);
    }
}
