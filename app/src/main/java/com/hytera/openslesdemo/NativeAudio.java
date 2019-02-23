package com.hytera.openslesdemo;

/**
 * Created by Administrator on 2017/11/9.
 */

public class NativeAudio {
    public native int startCap(String fileName, String recordtimefileName, int chanNum, int capStreamType, int playStreamType, int sampleRate, boolean isStressTest, int stressTestNum, boolean isSco);
    public native int stopCap();
    public native int startPlay(String fileName, String recordtimefileName, int playStreamType, int sampleRate, boolean isStressTest, int stressTestNum);
    public native int stopPlay();
    public native int startLoop(int chanNum, int capStreamType, int playStreamType, int sampleRate);
    public native int stopLoop();
}
