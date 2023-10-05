package com.khronos.player;

import android.graphics.SurfaceTexture;
import android.util.Log;
import android.view.Surface;

public class VideoSurface implements SurfaceTexture.OnFrameAvailableListener
{
    boolean mUpdateSurface=false;
    SurfaceTexture mSurfaceTex;
    Surface mSurface;   

    public void newSurfaceAndTex(int texIndexP)
    {
        this.mSurfaceTex=new SurfaceTexture(texIndexP);
        this.mSurfaceTex.setOnFrameAvailableListener(this);
        this.mSurface=new Surface(this.mSurfaceTex);

        setSurface(mSurface);   
    }

    
    @Override
    public void onFrameAvailable(SurfaceTexture surface)
    {
        this.mUpdateSurface=true;
    }

    public void curSurfaceTexUpdate()
    {
        if(this.mUpdateSurface) {
            this.mSurfaceTex.updateTexImage();
            this.mUpdateSurface=false;
        }
    }

    public static native void setSurface(Surface surface);

}
