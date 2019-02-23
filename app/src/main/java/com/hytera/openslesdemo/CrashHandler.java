package com.hytera.openslesdemo;

import java.io.File;
import java.io.FileOutputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.io.Writer;
import java.lang.Thread.UncaughtExceptionHandler;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;

import android.os.Environment;
import android.util.Log;

public class CrashHandler implements UncaughtExceptionHandler{
	private static final String TAG = "CrashHandler";
	private static CrashHandler mCrashHandler;     
    private UncaughtExceptionHandler mDefaultHandler;
    private DateFormat mFormatter;  
   
    private CrashHandler() {
    	mFormatter = new SimpleDateFormat("yyyy-MM-dd-HH-mm-ss"); 
    }  
  
    public static CrashHandler getInstance() {
    	if (null == mCrashHandler) {
    		mCrashHandler = new CrashHandler();
    	}
        return mCrashHandler;  
    }  
  
    public void init() {
        mDefaultHandler = Thread.getDefaultUncaughtExceptionHandler();        
        Thread.setDefaultUncaughtExceptionHandler(this);
        Log.e(TAG, "init success");
    }  
   
    @Override  
    public void uncaughtException(Thread thread, Throwable ex) {  
        if (!handleException(ex) && mDefaultHandler != null) {  
            mDefaultHandler.uncaughtException(thread, ex);  
        }
    }  
   
    private boolean handleException(Throwable ex) {  
        if (ex == null) {  
            return false;  
        }
        saveCrashInfo2File(ex);	     
        return true;  
    } 
	
	private String saveCrashInfo2File(Throwable ex) {  
        StringBuffer sb = new StringBuffer();  
        Writer writer = new StringWriter();  
        PrintWriter printWriter = new PrintWriter(writer);  
        ex.printStackTrace(printWriter);  
        Throwable cause = ex.getCause();  
        while (cause != null) {  
            cause.printStackTrace(printWriter);  
            cause = cause.getCause();  
        }  
        printWriter.close();  
        String result = writer.toString();  
        sb.append(result);  
        try {  
            long timestamp = System.currentTimeMillis();  
            String time = mFormatter.format(new Date());  
            String fileName = "crash-" + time + "-" + timestamp + ".log";  
              
            if (Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {  
                String path = "/sdcard/OpenSlesDemo/crash/";
                File dir = new File(path);  
                if (!dir.exists()) {  
                    dir.mkdirs();  
                }  
                FileOutputStream fos = new FileOutputStream(path + fileName);  
                fos.write(sb.toString().getBytes());  
                fos.close();  
            }  
            return fileName;  
        } catch (Exception e) {  
            Log.e(TAG, "an error occured while writing file...", e);  
        }  
        return null;  
    }
}
