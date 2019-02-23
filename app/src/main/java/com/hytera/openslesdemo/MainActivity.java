package com.hytera.openslesdemo;

import android.Manifest;
import android.app.Activity;
import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHeadset;
import android.bluetooth.BluetoothProfile;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.telephony.PhoneStateListener;
import android.telephony.TelephonyManager;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.text.SimpleDateFormat;
import java.util.Date;

import static android.os.Build.VERSION.SDK_INT;
import static android.telephony.TelephonyManager.ACTION_PHONE_STATE_CHANGED;

public class MainActivity extends Activity implements View.OnClickListener{
    private static final String TAG = "OPENSLESDEMO-MAIN";
    private static final int STAT_IDLE = 0x0;
    private static final int STAT_RECODE = 0x1;
    private static final int STAT_PLAY = 0x2;
    private static final int STAT_LOOP = 0x3;
    private static final int CAP_STREAM_TYPE_GENERIC = 0x1;
    private static final int CAP_STREAM_TYPE_CAMCORDER = 0x2;
    private static final int CAP_STREAM_TYPE_VOICE_RECOGNITION = 0x3;
    private static final int CAP_STREAM_TYPE_VOICE_COMMUNICATION = 0x4;
    private static final int Play_STREAM_TYPE_MEDIA= 0x3;
    private TextView mDisplay;
    private Spinner mCapStreamType;
    private Spinner mPlayStreamType;
    private Spinner mSampleRate;
    private Spinner mChan;
    private Spinner mStressTestCount;
    private Button mStartCap;
    private Button mStopCap;
    private Button mStartPlay;
    private Button mStopPlay;
    private Button mStartLoop;
    private Button mStopLoop;
    //private Button mSco;
    private Switch mStressTest;
    private Switch mAndroidPlay;
    private NativeAudio mNativeAudio;
    private boolean mIsStressTest = false;
	private boolean mIsAndroidPlay = false;
    private int mSR = 16000;
    private int mChanNum = 0;
    private int mCSType = 0;
    private int mPSType = 0;
    private int mState;
    private int mStressTestNum = 0;
    private StringBuilder mSBstate;
    private StringBuilder mFileName;
	private StringBuilder mRecordTimeFileName;
    private StringBuffer mResult;
    private Thread mCapThread;
    private Thread mPlayThread;
    private Thread mLoopThread;
    private Handler mHanler = null;
    private IntentFilter mBluetoothHeadsetFilter = null;
    private IntentFilter mBluetoothScoFilter = null;
    private IntentFilter mPhoneFilter = null;
    private AudioManager mAudioManager = null;
    private boolean mHasBlueToothHeadset = false;
    private boolean mHasBlueToothSco = false;

    private int mDataLen;
    private int mBufferSizeInBytes = 0;
    private int mgap = 20;
    private int mbps = 16;
	private boolean mIsRun = false;
    //private int mChanNum = AudioFormat.CHANNEL_OUT_STEREO;
    private int mChanNumber = AudioFormat.CHANNEL_OUT_MONO;
    private int mAudioFormat = AudioFormat.ENCODING_PCM_16BIT;
    private int mStreamType = AudioManager.STREAM_MUSIC;
    private AudioTrack mAudioTrack = null;

    private FileOutputStream mFos = null;
    private FileInputStream mFin = null;
    private File mPlayFileName = null;
    private InputStream mSrc = null;

    private String[] mDirs = {
            "/sdcard/OpenSlesDemo/play/",
            "/sdcard/OpenSlesDemo/record/"
    };
    private String[] mPlayFiles = {
            "/sdcard/OpenSlesDemo/play/play8kMono.pcm",
            "/sdcard/OpenSlesDemo/play/play16kMono.pcm"
    };
    /*
    private BroadcastReceiver mBlueToothheadsetReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            Log.e(TAG,"BlueToothheadsetReceiver  enter 1 action:" + action);
            try {
                Thread.sleep(100);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
            Log.e(TAG,"BlueToothheadsetReceiver  enter 2 action:" + action);
            if (BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED.equals(action)) {
                BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
                Log.e(TAG,"BlueToothheadsetReceiver  enter action:" + action + " HEADSET:" + adapter.getProfileConnectionState(BluetoothProfile.HEADSET) +
                        " A2DP:" + adapter.getProfileConnectionState(BluetoothProfile.A2DP) + " HEALTH:" + adapter.getProfileConnectionState(BluetoothProfile.HEALTH));
                if(BluetoothProfile.STATE_DISCONNECTED == adapter.getProfileConnectionState(BluetoothProfile.HEADSET)) {
                    Log.e(TAG,"BlueToothheadsetReceiver  bluetooth disconnected");
                }else if(BluetoothProfile.STATE_CONNECTED == adapter.getProfileConnectionState(BluetoothProfile.HEADSET)) {
                    Log.e(TAG, "BlueToothheadsetReceiver	bluetooth connected");
                }else {
                    Log.e(TAG, "BlueToothheadsetReceiver  ACTION_CONNECTION_STATE_CHANGED:" + adapter.getProfileConnectionState(BluetoothProfile.HEADSET));
                }
            }else {
                Log.e(TAG, "BlueToothheadsetReceiver  action:" + action);
            }
        }
    };
    */

    private BroadcastReceiver mPhoneReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            Log.e(TAG,"action " + intent.getAction());
            if(intent.getAction().equals(Intent.ACTION_NEW_OUTGOING_CALL)){
                String phoneNumber = intent.getStringExtra(Intent.EXTRA_PHONE_NUMBER);
                Log.e(TAG,"call out, remote phone number is :" + phoneNumber);
            }else {
                TelephonyManager tm = (TelephonyManager)context.getSystemService(Service.TELEPHONY_SERVICE);
                tm.listen(mPhoneStateListener, PhoneStateListener.LISTEN_CALL_STATE);
            }
        }
    };
    PhoneStateListener mPhoneStateListener = new PhoneStateListener() {
        @Override
        public void onCallStateChanged(int state, String incomingNumber) {
            super.onCallStateChanged(state, incomingNumber);
            switch(state) {
                case TelephonyManager.CALL_STATE_IDLE: {
                    Log.e(TAG,"idle");
                    break;
                }
                case TelephonyManager.CALL_STATE_OFFHOOK: {
                    Log.e(TAG,"offhook");
                    break;
                }
                case TelephonyManager.CALL_STATE_RINGING: {
                    Log.e(TAG,"ringing: remote phone number is " + incomingNumber);
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
    };
    private BroadcastReceiver mBlueToothheadsetReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            //Log.e(TAG,"BlueToothheadsetReceiver  action:" + action);
            int state = intent.getIntExtra(BluetoothProfile.EXTRA_STATE, BluetoothHeadset.STATE_DISCONNECTED);
            int preState = intent.getIntExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, BluetoothHeadset.STATE_DISCONNECTED);
            BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            int deviceType = device.getBluetoothClass().getMajorDeviceClass();
            Log.e(TAG, "BlueToothheadsetReceiver  state:" + state + " preState:" + preState + " deviceType:" + deviceType + " name:" + device.getName());
            if (1024 == deviceType) {
                if(state == BluetoothHeadset.STATE_CONNECTED) {
					new Thread(new Runnable() {
                        @Override
                        public void run() {
                            try {
                                Thread.sleep(1500);
                            } catch (InterruptedException e) {
                                e.printStackTrace();
                            }
							mAudioManager.startBluetoothSco();
                        }
                    }).start();
                    mHasBlueToothHeadset = true;
                }else if(state == BluetoothHeadset.STATE_DISCONNECTED) {
                    mHasBlueToothHeadset = false;
                    mAudioManager.setBluetoothScoOn(false);
                    mAudioManager.stopBluetoothSco();
                    Log.e(TAG, "BlueToothheadsetReceiver  stopBluetoothSco");
                }
            }
        }
    };

    private BroadcastReceiver mBlueToothScoReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (!mHasBlueToothHeadset && !mHasBlueToothSco) {
                Log.e(TAG, "receiver sco broadcast, but mHasBlueToothHeadset and mHasBlueToothSco is false, do not deal!");
                return;
            }
            String action = intent.getAction();
            if(AudioManager.ACTION_SCO_AUDIO_STATE_UPDATED.equals(action)) {
                int status = intent.getIntExtra(AudioManager.EXTRA_SCO_AUDIO_STATE, AudioManager.SCO_AUDIO_STATE_ERROR );
                Log.e(TAG, "BT SCO state changed : " + status + " mHasBlueToothSco:" + mHasBlueToothSco);
                if(status == AudioManager.SCO_AUDIO_STATE_CONNECTED) {
                    mAudioManager.setMode(AudioManager.MODE_IN_COMMUNICATION);
                    mAudioManager.setSpeakerphoneOn(false);
                    mAudioManager.setBluetoothScoOn(true);
                    mHasBlueToothSco = true;
                    Message msg = new Message();
                    msg.obj="sco on";
                    mHanler.sendMessage(msg);
                }else if(status == AudioManager.SCO_AUDIO_STATE_DISCONNECTED) {
                    mAudioManager.setMode(AudioManager.MODE_NORMAL);
                    //mAudioManager.setBluetoothScoOn(false);
                    mAudioManager.setSpeakerphoneOn(true);
					if (mHasBlueToothSco) {
                        mHasBlueToothSco = false;
                        Message msg = new Message();
                        msg.obj = "sco off";
                        mHanler.sendMessage(msg);
                    }else {
                        Log.e(TAG,"do not display");
                    }
                }else {
                    Log.e(TAG, "lideshou: ### status:" + status);
                    return;
                }
            }
        }
    };
    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.e(TAG, "onCreate");
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        init();;
        Log.e(TAG, "onCreate  " + getApplicationInfo().nativeLibraryDir);
    }

    @Override
    protected void onRestart() {
        Log.e(TAG, "onRestart");
        super.onRestart();
        //init();
    }


    @Override
    protected void onStart() {
        Log.e(TAG, "onStart");
        super.onStart();
    }

    @Override
    protected void onResume() {
        Log.e(TAG, "onResume");
        super.onResume();
    }

    @Override
    protected void onPause() {
        Log.e(TAG, "onPause");
        super.onPause();
    }

    @Override
    protected void onStop() {
        Log.e(TAG, "onStop  mState:" + ApkStateToString(mState) + " mHasBlueToothSco:" + BooleanToString(mHasBlueToothSco));
		if (STAT_IDLE == mState) {
			if (mHasBlueToothSco) {
				mAudioManager.setBluetoothScoOn(false);			
	            mAudioManager.stopBluetoothSco();
			}
		}
		Log.e(TAG, "onStop  success");
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        Log.e(TAG, "onDestroy enter");
        stopCap();
		Log.e(TAG, "onDestroy stopCap success");
        stopPlay();
		Log.e(TAG, "onDestroy stopPlay success");
        stopLoop();
		Log.e(TAG, "onDestroy stopLoop success");
		if (mHasBlueToothSco) {
			mAudioManager.setBluetoothScoOn(false);			
            mAudioManager.stopBluetoothSco();
		}
        unregisterReceiver(mBlueToothScoReceiver);
		Log.e(TAG, "onDestroy unregisterReceiver mBlueToothScoReceiver success");
        unregisterReceiver(mBlueToothheadsetReceiver);
        Log.e(TAG, "onDestroy unregisterReceiver mBlueToothheadsetReceiver success");
        unregisterReceiver(mPhoneReceiver);
        Log.e(TAG, "onDestroy unregisterReceiver mPhoneReceiver success");
        super.onDestroy();
    }

    private void checkBluetoothHeadset() {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        boolean isUseBlutTooth = adapter.isEnabled();
        if (isUseBlutTooth) {
            if (BluetoothProfile.STATE_CONNECTED == adapter.getProfileConnectionState(BluetoothProfile.HEADSET)) {
                Log.e(TAG, "checkBluetoothHeadset  bluetooth headset connected");
                mHasBlueToothHeadset = true;
				mAudioManager.startBluetoothSco();
            }
        }else {
            Log.e(TAG, "checkBluetoothHeadset  bluetooth is not enable");
        }
    }
    private int startCap() {
        int len = mFileName.length();
        mFileName.delete(0, len);

        len = mRecordTimeFileName.length();
		mRecordTimeFileName.delete(0, len);

        mCSType = mCapStreamType.getSelectedItemPosition();
		mPSType = mPlayStreamType.getSelectedItemPosition();

        if (0 == mChan.getSelectedItemPosition()) {
            mChanNum = 2;
        }else {
            mChanNum = 1;
        }
        SimpleDateFormat format = new SimpleDateFormat("yyyy-MM-dd-HH-mm-ss");
        String str = format.format(new Date());
        if (mSR == 16000) {
            if (2 == mChanNum) {
                mFileName.append("/sdcard/OpenSlesDemo/record/OpenSlesRecordStereo16000-" + CaptreamTypeToString(mCSType) + "-" + str + ".pcm");
				mRecordTimeFileName.append("/sdcard/OpenSlesDemo/record/OpenSlesRecordStereo16000-" + CaptreamTypeToString(mCSType) + "-" + str + ".log");
            }else {
                mFileName.append("/sdcard/OpenSlesDemo/record/OpenSlesRecordMono16000-" + CaptreamTypeToString(mCSType) + "-" + str + ".pcm");
				mRecordTimeFileName.append("/sdcard/OpenSlesDemo/record/OpenSlesRecordMono16000-" + CaptreamTypeToString(mCSType) + "-" + str + ".log");
            }
        }else {
            if (2 == mChanNum) {
                mFileName.append("/sdcard/OpenSlesDemo/record/OpenSlesRecordStereo8000-" + CaptreamTypeToString(mCSType) + "-" + str + ".pcm");
				mRecordTimeFileName.append("/sdcard/OpenSlesDemo/record/OpenSlesRecordStereo8000-" + CaptreamTypeToString(mCSType) + "-" + str + ".log");
            }else {
                mFileName.append("/sdcard/OpenSlesDemo/record/OpenSlesRecordMono8000-" + CaptreamTypeToString(mCSType) + "-" + str + ".pcm");
				mRecordTimeFileName.append("/sdcard/OpenSlesDemo/record/OpenSlesRecordMono8000-" + CaptreamTypeToString(mCSType) + "-" + str + ".log");
            }
        }
        Log.e(TAG, "start create record file:" + mFileName.toString());
        File capFile = new File(mFileName.toString());
        if(!capFile.exists())  {
            try {
                capFile.createNewFile();
            } catch (IOException e) {
                e.printStackTrace();
                return -1;
            }
            capFile.setWritable(true);
        }
        mCapThread = new Thread(new CapThread());
        mCapThread.setName("audioCapThread");
        mState = STAT_RECODE;
        mCapThread.start();
        return 0;
    }

    private int stopCap() {
		int ret = 0;
		if (STAT_RECODE == mState) {
		    ret = mNativeAudio.stopCap();
		    if (ret < 0) {
		        Log.e(TAG, "stopCap fail! ret = " + ret);
		        return -1;
		    }
		    if (null != mCapThread) {
				if (mCapThread.isAlive()) {
		            try {
		                mCapThread.join(1000);
		            } catch (InterruptedException e) {
		                e.printStackTrace();
		            }
				}
				mCapThread = null;
		    }
		    mState = STAT_IDLE;
		}
        return ret;
    }

    private int startPlay() {
        int len = mFileName.length();
        mFileName.delete(0, len);

        len = mRecordTimeFileName.length();
		mRecordTimeFileName.delete(0, len);

        mPSType = mPlayStreamType.getSelectedItemPosition();

        SimpleDateFormat format = new SimpleDateFormat("yyyy-MM-dd-HH-mm-ss");
        String str = format.format(new Date());

        if (mSR == 16000) {
            mFileName.append("/sdcard/OpenSlesDemo/play/play16kMono.pcm");
			mRecordTimeFileName.append("/sdcard/OpenSlesDemo/play/play16kMono-" + PlaytreamTypeToString(mPSType) + "-" + str + ".log");
        }else {
            mFileName.append("/sdcard/OpenSlesDemo/play/play8kMono.pcm");
			mRecordTimeFileName.append("/sdcard/OpenSlesDemo/play/play8kMono-" + PlaytreamTypeToString(mPSType) + "-" + str + ".log");
        }
        mPlayThread = new Thread(new PlayThread());
        mPlayThread.setName("audioPlayThread");
        mState = STAT_PLAY;
        mPlayThread.start();
        return 0;
    }

    private int stopPlay() {
		int ret = 0;
		if (STAT_PLAY == mState) {
			if (mIsAndroidPlay) {
				mIsRun = false;
			}else {
	            ret = mNativeAudio.stopPlay();
			}
	        if (ret < 0) {
	            Log.e(TAG, "stopPlay fail! ret = " + ret);
	        }
	        if (null != mPlayThread) {
				if (mPlayThread.isAlive()) {
		            try {
		                mPlayThread.join(1000);
		            } catch (InterruptedException e) {
		                e.printStackTrace();
		            }
				}
				mPlayThread = null;
	        }
	        mState = STAT_IDLE;
		}
        return 0;
    }
    private int startLoop() {
        mPSType = mPlayStreamType.getSelectedItemPosition();
        mCSType = mCapStreamType.getSelectedItemPosition();
        if (0 == mChan.getSelectedItemPosition()) {
            mChanNum = 2;
        }else {
            mChanNum = 1;
        }
        mLoopThread = new Thread(new LoopThread());
        mLoopThread.setName("audioLoopThread");
        mState = STAT_LOOP;
        mLoopThread.start();
        return 0;
    }

    private int stopLoop() {
		int ret = 0;
		if (STAT_LOOP == mState) {
	        ret = mNativeAudio.stopLoop();
	        if (ret < 0) {
	            Log.e(TAG, "stopLoop fail! ret = " + ret);
	            return -1;
	        }
	        if (null != mLoopThread) {
				if (mLoopThread.isAlive()) {
		            try {
		                mLoopThread.join(1000);
		            } catch (InterruptedException e) {
		                e.printStackTrace();
		            }
				}
				mLoopThread = null;
	        }
	        mState = STAT_IDLE;
		}
        return ret;
    }
    private void init() {
        mDisplay = findViewById(R.id.tv_display);
        mSampleRate = findViewById(R.id.sp_sampleRate);
        mCapStreamType = findViewById(R.id.sp_capStreamType);
        mPlayStreamType = findViewById(R.id.sp_playStreamType);
        mChan = findViewById(R.id.sp_chan);
        mStressTest = findViewById(R.id.sw_stressTest);
        mAndroidPlay = findViewById(R.id.sw_AndroidPlay);
        mStressTestCount = findViewById(R.id.sp_stressTestCount);
        mStartCap = findViewById(R.id.bt_startCap);
        mStopCap = findViewById(R.id.bt_stopCap);
        mStartPlay = findViewById(R.id.bt_startPlay);
        mStopPlay = findViewById(R.id.bt_stopPlay);
        mStartLoop = findViewById(R.id.bt_startLoop);
        mStopLoop = findViewById(R.id.bt_stopLoop);
        //mSco = findViewById(R.id.bt_sco);
        mStartCap.setOnClickListener(this);
        mStopCap.setOnClickListener(this);
        mStartPlay.setOnClickListener(this);
        mStopPlay.setOnClickListener(this);
        mStartLoop.setOnClickListener(this);
        mStopLoop.setOnClickListener(this);
        //mSco.setOnClickListener(this);
        mState = STAT_IDLE;
        mSBstate = new StringBuilder(128);
        mFileName = new StringBuilder(512);
		mRecordTimeFileName = new StringBuilder(512);
        mResult = new StringBuffer(128);
        mNativeAudio = new NativeAudio();
        mAudioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        int ret = ProDealCapAndPlayFile(mDirs, mPlayFiles);
        switch (ret) {
            case 0:
            {
                mDisplay.setText("init success");
                break;
            }
            case -1:
            case -2:
            case -3:
            case -4:
            {
                mDisplay.setText("ProDealCapAndPlayFile fail; ret = " + ret);
                break;
            }
            default:
                break;
        }

		checkBluetoothHeadset();

        if (null == mBluetoothHeadsetFilter) {
            mBluetoothHeadsetFilter = new IntentFilter();
        }
        mBluetoothHeadsetFilter.addAction(BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED);
		/*
		mBluetoothFilter.addAction(BluetoothDevice.ACTION_ACL_CONNECTED);
		mBluetoothFilter.addAction(BluetoothDevice.ACTION_ACL_DISCONNECTED);
	    */
        registerReceiver(mBlueToothheadsetReceiver, mBluetoothHeadsetFilter);

        if (null == mBluetoothScoFilter) {
            mBluetoothScoFilter = new IntentFilter();
        }
        mBluetoothScoFilter.addAction(AudioManager.ACTION_SCO_AUDIO_STATE_UPDATED);
        registerReceiver(mBlueToothScoReceiver, mBluetoothScoFilter);

        if (null == mPhoneFilter) {
            mPhoneFilter = new IntentFilter();
        }
        mPhoneFilter.addAction(ACTION_PHONE_STATE_CHANGED);
        mPhoneFilter.addAction(Intent.ACTION_NEW_OUTGOING_CALL);
        registerReceiver(mPhoneReceiver, mPhoneFilter);

        if (null == mHanler) {
            mHanler =new Handler(){
                public void handleMessage(Message msg) {
                    mState = STAT_IDLE;
                    String str = (String)msg.obj;
					/*
                    if (str.contains("sco on")) {
                        mSco.setText("unUseSco");
                    }else if (str.contains("sco off")) {
                        mSco.setText("UseSco");
                    }
                    */
                    Log.e(TAG, "str: " + str);
                    mDisplay.setText(str);
                }
            };
        }

        mCapStreamType.setSelection(CAP_STREAM_TYPE_GENERIC);
        mPlayStreamType.setSelection(Play_STREAM_TYPE_MEDIA);

		//mAudioManager.setMode(AudioManager.MODE_IN_CALL);
        mAudioManager.setMode(AudioManager.MODE_NORMAL);
        //mAudioManager.setMode(AudioManager.MODE_IN_COMMUNICATION);
        /*
        if (android.os.Build.MODEL.contains("instk8735_6ttb_c")) {
            mAudioManager.setParameters("SetBesLoudnessStatus=0");
            Log.e(TAG, "close instk8735_6ttb_c BesLoudness");
        }
        */
        /*
        if(SystemProperties.getBoolean("persist.sys.bes_loud",true)) {
            mAudioManager.setParameters("SetBesLoudnessStatus=0");
            SystemProperties.set("persist.sys.bes_loud","false");
        }
        */
    }

    private int ProDealCapAndPlayFile(String[] dirs, String[] files) {
        for (String tmp : dirs) {
            File dir = new File(tmp);
            if (!dir.exists()) {
                dir.mkdirs();
                dir.setWritable(true);
                dir.setReadable(true);
            }
        }

        for (String tmp : files) {
            File file = new File(tmp);
            if (!file.exists()) {
                InputStream is = null;
                if (tmp.contains("play8kMono.pcm")) {
                    is = getClass().getResourceAsStream("/assets/play8kMono.pcm");
                }else if (tmp.contains("play16kMono.pcm")) {
                    is = getClass().getResourceAsStream("/assets/play16kMono.pcm");
                }else {
                    //to do
                    Log.e(TAG, "current do not play " + tmp);
                    return -1;
                }
                file.setReadable(true);
                file.setWritable(true);

                FileOutputStream fos = null;
                try {
                    fos = new FileOutputStream(file);
                } catch (FileNotFoundException e) {
                    e.printStackTrace();
                    Log.e(TAG, "new FileOutputStream fail");
                    return -2;
                }
                byte[] data = new byte[1024];
                int len = 0;
                int count = 0;
                while (true) {
                    try {
                        len = is.read(data, 0, 1024);
                        count++;
                        if (0 >= len) {
                            Log.e(TAG, "file read complete! len: " + len + " count:" + count);
                            break;
                        }
                        fos.write(data, 0, len);
                    } catch (IOException e) {
                        e.printStackTrace();
                        Log.e(TAG, tmp + "write fail! count:" + count);
                        return -3;
                    }
                }
                Log.e(TAG, "ProDealCapAndPlayFile count:" + count);
                try {
                    fos.close();
                } catch (IOException e) {
                    e.printStackTrace();
                    Log.e(TAG, tmp + "close fail");
                    return -4;
                }
            }
        }
        return 0;
    }

    @Override
    public void onClick(View view) {
        mIsStressTest = mStressTest.isChecked();
        if (mIsStressTest) {
            mStressTestNum = Integer.parseInt(mStressTestCount.getSelectedItem().toString());
            Log.e(TAG, "mStressTestNum = " + mStressTestNum + " mIsStressTest = " + mIsStressTest);
        }
        mSR = Integer.parseInt(mSampleRate.getSelectedItem().toString());
        mIsAndroidPlay = mAndroidPlay.isChecked();
        Log.e(TAG, "sample rate is " + mSR + " mIsAndroidPlay:" + BooleanToString(mIsAndroidPlay));
		
        switch(view.getId()) {
            case R.id.bt_startCap:
            {
                if (mState == STAT_IDLE) {
                    if (mHasBlueToothHeadset && !mHasBlueToothSco) {
                        Toast.makeText(this, "bluetooth sco not connet, please click useSco", 5).show();
                        Log.e(TAG, "bluetooth sco not connet, please click useSco");
                        break;
                    }
                    int ret = startCap();
                    if (ret == 0) {
                        if (mIsStressTest) {
                            mDisplay.setText("start record stress test");
                        }else {
                            mDisplay.setText("start record! file is " + mFileName.toString());
                        }
                    }else {
                        mDisplay.setText("create record file fail! " + mFileName.toString());
                    }
                }else {
                    int len = mSBstate.length();
                    mSBstate.delete(0, len);
                    if (mState == STAT_RECODE) {
                        mSBstate.append("record");
                    }else {
                        mSBstate.append("play");
                    }
                    mDisplay.setText("can not record, current stat is " + mSBstate.toString());
                }
                break;
            }
            case R.id.bt_stopCap:
            {
                if (mState == STAT_RECODE) {
                    stopCap();
                    mDisplay.setText("stop record");
                }else {
                    int len = mSBstate.length();
                    mSBstate.delete(0, len);
                    if (mState == STAT_IDLE) {
                        mSBstate.append("idle");
                    }else {
                        mSBstate.append("play");
                    }
                    mDisplay.setText("can not stop record, current stat is " + mSBstate.toString());
                }
                break;
            }
            case R.id.bt_startPlay:
            {
                if (mState == STAT_IDLE) {
                    int ret = startPlay();
                    if (ret == 0) {
                        if (mIsStressTest) {
                            mDisplay.setText("start play stress test");
                        }else {
                            mDisplay.setText("start play");
                        }
                    }else {
                        mDisplay.setText("get play pcm file fail!");
                    }
                }else {
                    int len = mSBstate.length();
                    mSBstate.delete(0, len);
                    if (mState == STAT_RECODE) {
                        mSBstate.append("record");
                    }else {
                        mSBstate.append("play");
                    }
                    mDisplay.setText("can not play, current stat is " + mSBstate.toString());
                }
                break;
            }
            case R.id.bt_stopPlay:
            {
                if (mState == STAT_PLAY) {
                    stopPlay();
                    mDisplay.setText("stop play");
                }else {
                    int len = mSBstate.length();
                    mSBstate.delete(0, len);
                    if (mState == STAT_IDLE) {
                        mSBstate.append("idle");
                    }else {
                        mSBstate.append("record");
                    }
                    mDisplay.setText("can not stop play, current stat is " + mSBstate.toString());
                }
                break;
            }
            case R.id.bt_startLoop:
            {
                if (mState == STAT_IDLE) {
                    int ret = startLoop();
                    if (ret == 0) {
                        mDisplay.setText("start loop");
                    }else {
                        mDisplay.setText("startLoop fail!");
                    }
                }else {
                    int len = mSBstate.length();
                    mSBstate.delete(0, len);
                    if (mState == STAT_RECODE) {
                        mSBstate.append("record");
                    }else if(mState == STAT_PLAY){
                        mSBstate.append("play");
                    }else {
                        mSBstate.append("loop");
                    }
                    mDisplay.setText("can not loop, current stat is " + mSBstate.toString());
                }
                break;
            }
            case R.id.bt_stopLoop:
            {
                if (mState == STAT_LOOP) {
                    stopLoop();
                    mDisplay.setText("stop loop");
                }else {
                    int len = mSBstate.length();
                    mSBstate.delete(0, len);
                    if (mState == STAT_IDLE) {
                        mSBstate.append("idle");
                    }else if(mState == STAT_RECODE) {
                        mSBstate.append("record");
                    }else {
                        mSBstate.append("loop");
                    }
                    mDisplay.setText("can not stop loop, current stat is " + mSBstate.toString());
                }
                break;
            }
            /*
            case R.id.bt_sco:
            {

                Log.e(TAG, "******** " + mSco.getText() + " mHasBlueToothHeadset:" + BooleanToString(mHasBlueToothHeadset));
                if (mState == STAT_IDLE && mHasBlueToothHeadset) {
                    if (mSco.getText().equals("UseSco") && !mHasBlueToothSco) {
                        Log.e(TAG, "******** startBluetoothSco");
                        mAudioManager.startBluetoothSco();
                        mSco.setText(R.string.unUseSco);
                    } else {
                        mAudioManager.stopBluetoothSco();
                        mSco.setText(R.string.useSco);
                    }
                }else {
                    Toast.makeText(this, "can not deal sco", 5).show();
                    Log.e(TAG, "can not deal sco  mState:" + ApkStateToString(mState) + " mHasBlueToothHeadset:" + BooleanToString(mHasBlueToothHeadset));
                }

            }
            */
            default:
            {
                Log.e(TAG,"no defile this button");
                break;
            }
        }
    }

    public int androidPlay() {
		mBufferSizeInBytes = AudioTrack.getMinBufferSize(mSR, mChanNumber, mAudioFormat);
		if (mBufferSizeInBytes < GetBufLen(200)) {
			mBufferSizeInBytes = GetBufLen(200);
		}

        mAudioTrack = new AudioTrack(mStreamType, mSR, AudioFormat.CHANNEL_OUT_MONO, mAudioFormat, mBufferSizeInBytes, AudioTrack.MODE_STREAM);
		if (null == mAudioTrack) {
			Log.e(TAG, "androidPlay:create AudioTrack fail");
			return -1;
        }else {
        	Log.e(TAG, "androidPlay:create AudioTrack success"); 
        }
		int maxVol = mAudioManager.getStreamMaxVolume(mStreamType);
		int curVol = mAudioManager.getStreamVolume(mStreamType);
		Log.e(TAG, "androidPlay:maxVol is " + maxVol + " curVol is " + curVol); 

        mDataLen = GetFrameDataLen();
        byte[] audiodata = new byte[mDataLen];
        int readsize = 0;
        int len = 0;
        if (null == mFin) {
			File file = null;
            if (16000 == mSR) {
                file = new File(mPlayFiles[1]);
            }
            if (8000 == mSR) {
                file = new File(mPlayFiles[0]);
            }
            try {
                mFin = new FileInputStream(file);
            } catch (FileNotFoundException e) {
                e.printStackTrace();
                Log.e(TAG, "androidPlay: IOException " + e.getLocalizedMessage());
                return -1;
            }
            try {
                len = mFin.available();
            } catch (IOException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
                Log.e(TAG, "androidPlay: IOException " + e.getLocalizedMessage());
                return -1;
            }
        }

		Log.e(TAG, "androidPlay:mBufferSizeInBytes:" + mBufferSizeInBytes + " mDataLen:" + mDataLen + " len:" + len); 
		mIsRun = true;
        mAudioTrack.play();
        while (mIsRun) {
            try {
                readsize = mFin.read(audiodata);
                if (mDataLen != readsize) {
                    Log.e(TAG, "androidPlay:read fin file fail! readsize = " + readsize + " mDataLen = " + mDataLen);
                    try {
                        mFin.close();
                    } catch (IOException e) {
                        // TODO Auto-generated catch block
                        e.printStackTrace();
                        Log.d(TAG, "androidPlay: IOException " + e.getLocalizedMessage());
                    }
                    mFin = null;
                    break;
                }
            } catch (IOException e) {
                // TODO Auto-generated catch block
                Log.e(TAG, "androidPlay:IOException read fin file fail! "  + e.getLocalizedMessage());
                e.printStackTrace();
            }
			//Log.e(TAG, "androidPlay: readsize = " + readsize + " mDataLen = " + mDataLen);
            mAudioTrack.write(audiodata, 0, readsize);
        }
		if (null != mFin) {
            try {
                mFin.close();
            } catch (IOException e) {
                e.printStackTrace();
                Log.e(TAG, "androidPlay:IOException close file fail! "  + e.getLocalizedMessage());
            }
            mFin = null;
		}
        mAudioTrack.stop(); 
		mAudioTrack.release();
		mAudioTrack = null; 
		Log.e(TAG, "androidPlay: end");
        return 0;
    }
	
    public class CapThread implements Runnable {
        @Override
        public void run() {
            Log.e(TAG, "CapThread fileName:" + mFileName.toString() + " mChanNum:" + ChanToString(mChanNum) + " " + mChanNum + " mCSType:" + CaptreamTypeToString(mCSType)  + " " + mCSType + " SR:" + mSR + " mIsStressTest:" + mIsStressTest + " mStressTestNum:" + mStressTestNum + " mHasBlueToothSco:" + mHasBlueToothSco);
            int ret = mNativeAudio.startCap(mFileName.toString(), mRecordTimeFileName.toString(), mChanNum, mCSType, mPSType, mSR, mIsStressTest, mStressTestNum, mHasBlueToothSco);
            if (ret < 0) {
                Log.e(TAG, "startCap fail: ret = " + ret);
            }
            ParseTestResult("record", ret);
            Log.e(TAG, "CapThread end! ret = " + ret);
        }
    }

    public class PlayThread implements Runnable {
        @Override
        public void run() {
            Log.e(TAG, "PlayThread fileName:" + mFileName.toString() + " mPSType:" + PlaytreamTypeToString(mPSType) + " " + mPSType + " SR:" + mSR + " mIsStressTest:" + mIsStressTest + " mStressTestNum:" + mStressTestNum);
			int ret = 0;
			if (mIsAndroidPlay) {
				ret = androidPlay();
			}else {
                ret = mNativeAudio.startPlay(mFileName.toString(), mRecordTimeFileName.toString(), mPSType, mSR, mIsStressTest, mStressTestNum);
			}
            if (ret < 0) {
                Log.e(TAG, "startPlay fail: ret = " + ret);
            }
            ParseTestResult("play", ret);
            Log.e(TAG, "PlayThread end! ret = " + ret);
        }
    }

    public class LoopThread implements Runnable {
        @Override
        public void run() {
            Log.e(TAG, "LoopThread  mChanNum:" + ChanToString(mChanNum) + " " + mChanNum  + " mCSType:" + CaptreamTypeToString(mCSType) + " " + mCSType + "mPSType:" + PlaytreamTypeToString(mPSType) + " " + mPSType + " SR:" + mSR );
            int ret = mNativeAudio.startLoop(mChanNum, mCSType, mPSType, mSR);
            if (ret < 0) {
                Log.e(TAG, "startLoop fail: ret = " + ret);
            }
            ParseTestResult("loop", ret);
            Log.e(TAG, "LoopThread end! ret = " + ret);
        }
    }
    private void ParseTestResult(String type, int ret) {
        int len = mResult.length();
        mResult.delete(0, len);
        mResult.append(type);
        switch(ret) {
            case 0:
            {
                mResult.append(" test success!");
                if (type.contains("record")) {
                    mResult.append(" please get record file form /sdcard/OpenSlesDemo/record/");
                }
                break;
            }
            case -1:
            {
                mResult.append(" test fail! reason:open open file fail!");
                break;
            }
            case -2:
            {
                mResult.append(" test fail! reason:open opensles audio device fail!");
                break;
            }
            default:
            {
                mResult.append(" test result unkown!");
                break;
            }
        }
        Message msg = new Message();
        msg.obj=mResult.toString();
        mHanler.sendMessage(msg);
    }

    private int ChanFormatToNum(int chanformat) {
        int num = 0;
        if(AudioFormat.CHANNEL_OUT_STEREO == chanformat) {
            num = 2;
        }else if (AudioFormat.CHANNEL_OUT_MONO == chanformat) {
            num = 1;
        }
        return num;
    }

    private int GetFrameDataLen() {
        return ((mSR * mgap) * (mbps / 8) * ChanFormatToNum(mChanNumber)) / 1000;
    }

    private int GetBufLen(int timeLen) {
        if (timeLen < mgap) {
            return 0;
        }
        return ((mSR / 1000) * mgap) * (mbps / 8) * ChanFormatToNum(mChanNumber) * (timeLen / mgap);
    }

    public String BluteToothHeadsetStateToString(int state) {
        if (BluetoothHeadset.STATE_DISCONNECTED == state) {
            return "disconnected";
        }else if (BluetoothHeadset.STATE_CONNECTING == state) {
            return "connecting";
        }else if (BluetoothHeadset.STATE_CONNECTED == state) {
            return "connected";
        }else {
            return "disconnecting";
        }
    }

    public String BluteToothScoStateToString(int state) {
        if (AudioManager.SCO_AUDIO_STATE_DISCONNECTED == state) {
            return "sco-disconnected";
        }else if (AudioManager.SCO_AUDIO_STATE_CONNECTING == state) {
            return "sco-connecting";
        }else if (AudioManager.SCO_AUDIO_STATE_CONNECTED == state) {
            return "sco-connected";
        }else {
            return "sco-error";
        }
    }

    public String ApkStateToString(int state) {
        if (STAT_IDLE == state) {
            return "idle";
        }else if (STAT_PLAY == state) {
            return "play";
        }else if (STAT_RECODE == state) {
            return "record";
        }else if (STAT_LOOP == state) {
            return "loop";
        }
        return "";
    }
    public String BooleanToString(boolean is) {
        if (is) {
            return "true";
        }else{
            return "false";
        }
    }

    public String ChanToString(int chan) {
        if (2 == chan) {
            return "Stereo";
        }else if(1 == chan) {
            return "Mono";
        }else {
            return "";
        }
    }

    public String CaptreamTypeToString(int type) {
        if (0 == type) {
            return "SL_ANDROID_RECORDING_PRESET_NONE";
        }else if (1 == type) {
            return "SL_ANDROID_RECORDING_PRESET_GENERIC";
        }else if (2 == type) {
            return "SL_ANDROID_RECORDING_PRESET_CAMCORDER";
        }else if (3 == type) {
            return "SL_ANDROID_RECORDING_PRESET_VOICE_RECOGNITION";
        }else if (4 == type) {
            return "SL_ANDROID_RECORDING_PRESET_VOICE_COMMUNICATION";
        }else {
            return "";
        }
    }

    public String PlaytreamTypeToString(int type) {
        if (0 == type) {
            return "SL_ANDROID_STREAM_VOICE";
        }else if (1 == type) {
            return "SL_ANDROID_STREAM_SYSTEM";
        }else if (2 == type) {
            return "SL_ANDROID_STREAM_RING";
        }else if (3 == type) {
            return "SL_ANDROID_STREAM_MEDIA";
        }else if (4 == type) {
            return "SL_ANDROID_STREAM_ALARM";
        }else if (5 == type) {
            return "SL_ANDROID_STREAM_NOTIFICATION";
        }else {
            return "";
        }
    }
}
