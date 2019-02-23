package com.hytera.openslesdemo;

import java.io.File;

import android.app.ActivityManager;
import android.app.Application;
import android.content.Context;
import android.os.StatFs;
import android.util.Log;

public class App extends Application{
	private static final String TAG = "OPENSLESDEMO-APP";
	public void onCreate() {
		super.onCreate();Log.e(TAG, " " + getCurProcessName(this));
    	Log.e(TAG, " " + this.getPackageName());
    	Log.e(TAG, " " + android.os.Process.myPid());
    	CrashHandler.getInstance().init();
    	System.gc();
    }
    public long getTotalInternalMemorySize() {
		File path = new File("/sdcard");
		StatFs stat = new StatFs(path.getPath());
		long blockSize = stat.getBlockSize();
		long totalBlocks = stat.getBlockCount();
		return totalBlocks * blockSize;
	}

	String getCurProcessName(Context context) {
		int pid = android.os.Process.myPid();
		ActivityManager mActivityManager = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
		for (ActivityManager.RunningAppProcessInfo appProcess : mActivityManager.getRunningAppProcesses()) {
			if (appProcess.pid == pid)
				return appProcess.processName;
		}
		return null;
	}
}
