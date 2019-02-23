#include <jni.h>
#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <pthread.h>
#include <android/log.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <netinet/in.h>
#include <sys/system_properties.h>

#define LogD(...) __android_log_print(ANDROID_LOG_DEBUG,"OpenSlesDemo-JNI",__VA_ARGS__)
#define LogE(...) __android_log_print(ANDROID_LOG_ERROR,"OpenSlesDemo-JNI",__VA_ARGS__)

#define METHOD_COUNT(x) ((int) (sizeof(x) / sizeof((x)[0])))

static volatile int g_loop_exit = 0;
static volatile int g_stress_exit = 0;

static FILE * g_pCapRecordTimeFile = NULL;
static FILE * g_pPlayRecordTimeFile = NULL;

static const char *NATIVE_JNI_CLASS_PATH_NAME = "com/hytera/openslesdemo/NativeAudio";

static char g_szDevModel[128] = { 0 };
static char g_szFirmwareVer[128] = { 0 };
static int  g_FirstVer = 0;
static int  g_SecVer = 0;
typedef struct threadLock_ {
    pthread_mutex_t m;
    pthread_cond_t c;
    unsigned char s;
} threadLock;

extern "C" typedef struct opensl_stream {

    // engine interfaces
    SLObjectItf engineObject;
    SLEngineItf engineEngine;

    // output mix interfaces
    SLObjectItf outputMixObject;

    // buffer queue player interfaces
    SLObjectItf bqPlayerObject;
    SLPlayItf bqPlayerPlay;
	SLVolumeItf bqPlayerVol;
	//SLVolumeItf bqPlayerVol2;
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
    SLEffectSendItf bqPlayerEffectSend;

    // recorder interfaces
    SLObjectItf recorderObject;
    SLRecordItf recorderRecord;
    SLAndroidSimpleBufferQueueItf recorderBufferQueue;
	
	//SLAudioIODeviceCapabilitiesItf bqAudioIODeviceCapabilities;

    // buffer indexes
    int currentInputIndex;
    int currentOutputIndex;

    // current buffer half (0, 1)
    int currentOutputBuffer;
    int currentInputBuffer;

    // buffers
    short *outputBuffer[2];
    short *inputBuffer[2];

    // size of buffers
    int outBufSamples;
    int inBufSamples;

    // locks
    void*  inlock;
    void*  outlock;

    double time;
    int inchannels;
    int outchannels;
    int sr;
} OPENSL_STREAM;

static void* createThreadLock(void);
static void waitThreadLock(void *lock);
static void notifyThreadLock(void *lock);
static void destroyThreadLock(void *lock);
static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context);
static void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context);

/*
  Open the audio device with a given sampling rate (sr), input and output channels and IO buffer size
  in frames. Returns a handle to the OpenSL stream
*/
extern "C" OPENSL_STREAM* android_OpenAudioDevice(int type, int sr, int capStreamType, int playStreamType, int inchannels, int outchannels, int bufferframes, bool isSco);

/*
  Close the audio device
*/
extern "C" void android_CloseAudioDevice(OPENSL_STREAM *p);

/*
  Read a buffer from the OpenSL stream *p, of size samples. Returns the number of samples read.
*/
extern "C" int android_AudioIn(OPENSL_STREAM *p, short *buffer,int size);

/*
  Write a buffer to the OpenSL stream *p, of size samples. Returns the number of samples written.
*/
extern "C" int android_AudioOut(OPENSL_STREAM *p, short *buffer,int size);

/*
  Get the current IO block time in seconds
*/
extern "C" double android_GetTimestamp(OPENSL_STREAM *p);
// creates the OpenSL ES audio engine

static SLresult openSLCreateEngine(OPENSL_STREAM *p)
{
    SLresult result;
    // create engine
    result = slCreateEngine(&(p->engineObject), 0, NULL, 0, NULL, NULL);
    if(result != SL_RESULT_SUCCESS) {
		LogE("openSLCreateEngine slCreateEngine fail");
		return result;
	}

    // realize the engine
    result = (*p->engineObject)->Realize(p->engineObject, SL_BOOLEAN_FALSE);
    if(result != SL_RESULT_SUCCESS) {
		LogE("openSLCreateEngine Realize fail");
		return result;
	}

    // get the engine interface, which is needed in order to create other objects
    result = (*p->engineObject)->GetInterface(p->engineObject, SL_IID_ENGINE, &(p->engineEngine));
    if(result != SL_RESULT_SUCCESS) {
		LogE("openSLCreateEngine GetInterface fail");
	}

    return result;
}

// opens the OpenSL ES device for output
static SLresult openSLPlayOpen(OPENSL_STREAM *p, int playStreamType)
{
    SLresult result;
    SLuint32 sr = p->sr;
    SLuint32  channels = p->outchannels;

    memset(g_szDevModel, 0, sizeof(g_szDevModel));
    __system_property_get("ro.product.model", g_szDevModel);
    LogE("openSLPlayOpen sr:%d channels:%d playStreamType:%d g_szDevModel:%s", sr, channels, playStreamType, g_szDevModel);
	
    if ((strncmp(g_szDevModel, "PDC760", 6) == 0) || (strncmp(g_szDevModel, "PTC760", 6) == 0)) {
        memset(g_szFirmwareVer, 0, sizeof(g_szFirmwareVer));
        __system_property_get("ro.build.hytera.version", g_szFirmwareVer);
        LogE("openSLPlayOpen g_szFirmwareVer:%s", g_szFirmwareVer);
        if (strncmp(g_szFirmwareVer, "V", 1) == 0) {
            sscanf(g_szFirmwareVer + 1, "%d.%d", &g_FirstVer, &g_SecVer);
            LogE("openSLPlayOpen  g_FirstVer:%d g_SecVer:%d", g_FirstVer, g_SecVer);
        }
    }

    if(channels) {
        // configure audio source
        SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};

        switch(sr) {
            case 8000:
                sr = SL_SAMPLINGRATE_8;
                break;
            case 11025:
                sr = SL_SAMPLINGRATE_11_025;
                break;
            case 16000:
                sr = SL_SAMPLINGRATE_16;
                break;
            case 22050:
                sr = SL_SAMPLINGRATE_22_05;
                break;
            case 24000:
                sr = SL_SAMPLINGRATE_24;
                break;
            case 32000:
                sr = SL_SAMPLINGRATE_32;
                break;
            case 44100:
                sr = SL_SAMPLINGRATE_44_1;
                break;
            case 48000:
                sr = SL_SAMPLINGRATE_48;
                break;
            case 64000:
                sr = SL_SAMPLINGRATE_64;
                break;
            case 88200:
                sr = SL_SAMPLINGRATE_88_2;
                break;
            case 96000:
                sr = SL_SAMPLINGRATE_96;
                break;
            case 192000:
                sr = SL_SAMPLINGRATE_192;
                break;
            default:
                return -1;
        }
        /*
        outputMixObject do not support SL_IID_VOLUME interface
        */
        /*
        const SLInterfaceID id1[] = {SL_IID_VOLUME};
        const SLboolean req1[] = {SL_BOOLEAN_TRUE};
        result = (*p->engineEngine)->CreateOutputMix(p->engineEngine, &(p->outputMixObject), 1, id1, req1);
        */
        result = (*p->engineEngine)->CreateOutputMix(p->engineEngine, &(p->outputMixObject), 0, NULL, NULL);
        if(result != SL_RESULT_SUCCESS) {
            LogE("CreateOutputMix fail! result = %d", result);
            return result;
        }

        // realize the output mix
        result = (*p->outputMixObject)->Realize(p->outputMixObject, SL_BOOLEAN_FALSE);

        int speakers;
        if(channels > 1)
            speakers = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
        else speakers = SL_SPEAKER_FRONT_CENTER;

        SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, channels, sr,
                                       SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                       speakers, SL_BYTEORDER_LITTLEENDIAN};

        SLDataSource audioSrc = {&loc_bufq, &format_pcm};

        // configure audio sink
        SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, p->outputMixObject};
        SLDataSink audioSnk = {&loc_outmix, NULL};

        // create audio player
        /*
        SL_IID_AUDIOIODEVICECAPABILITIES do not support
        */
        /*
        const SLInterfaceID ids[4] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_VOLUME, SL_IID_ANDROIDCONFIGURATION, SL_IID_AUDIOIODEVICECAPABILITIES};
        const SLboolean req[4] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
        result = (*p->engineEngine)->CreateAudioPlayer(p->engineEngine, &(p->bqPlayerObject), &audioSrc, &audioSnk,
                                                       4, ids, req);
        */
		const SLInterfaceID ids[3] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_VOLUME, SL_IID_ANDROIDCONFIGURATION};
		const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
		result = (*p->engineEngine)->CreateAudioPlayer(p->engineEngine, &(p->bqPlayerObject), &audioSrc, &audioSnk,
													   3, ids, req);        
        if(result != SL_RESULT_SUCCESS) {
            LogE("CreateAudioPlayer fail! result = %d", result);
            return result;
        }

        /* begin; add by lideshou, 2017.12.11 */
        SLAndroidConfigurationItf playerConfig = NULL;
        /* audio small */
        //SLint32 streamType = SL_ANDROID_STREAM_VOICE;
        /* audio big */
        //SLint32 streamType = SL_ANDROID_STREAM_MEDIA;
        /* Set Android configuration */
        result = (*(p->bqPlayerObject))->GetInterface(p->bqPlayerObject,
                                                    SL_IID_ANDROIDCONFIGURATION,
                                                    &playerConfig);
        if (result == SL_RESULT_SUCCESS && playerConfig) {
            LogE("Warning: get playerConfig success");
            result = (*playerConfig)->SetConfiguration(
                    playerConfig, SL_ANDROID_KEY_STREAM_TYPE,
                    &playStreamType, sizeof(SLint32));
        }
        if (result != SL_RESULT_SUCCESS) {
            LogE("Warning: Unable to set android player configuration");
        }
        /* end */

        // realize the player
        result = (*p->bqPlayerObject)->Realize(p->bqPlayerObject, SL_BOOLEAN_FALSE);
        if(result != SL_RESULT_SUCCESS) {
            LogE("Realize fail! result = %d", result);
            return result;
        }

        // get the play interface
        result = (*p->bqPlayerObject)->GetInterface(p->bqPlayerObject, SL_IID_PLAY, &(p->bqPlayerPlay));
        if(result != SL_RESULT_SUCCESS) {
            LogE("GetInterface SL_IID_PLAY fail! result = %d", result);
            return result;
        }

        // get the buffer queue interface
        result = (*p->bqPlayerObject)->GetInterface(p->bqPlayerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                    &(p->bqPlayerBufferQueue));
        if(result != SL_RESULT_SUCCESS) {
            LogE("GetInterface SL_IID_ANDROIDSIMPLEBUFFERQUEUE fail! result = %d", result);
            return result;
        }

        /*
		LogE("GetInterface SL_IID_AUDIOIODEVICECAPABILITIES start");
        result = (*p->bqPlayerObject)->GetInterface(p->bqPlayerObject, SL_IID_AUDIOIODEVICECAPABILITIES,
                                                    &(p->bqAudioIODeviceCapabilities));
        if(result != SL_RESULT_SUCCESS) {
            LogE("GetInterface SL_IID_AUDIOIODEVICECAPABILITIES fail! result = %d", result);
            return result;
        }
		LogE("GetInterface SL_IID_AUDIOIODEVICECAPABILITIES success");
        */

		/* Get the volume interface */
        result = (*p->bqPlayerObject)->GetInterface(p->bqPlayerObject, SL_IID_VOLUME, &p->bqPlayerVol);
        if(result != SL_RESULT_SUCCESS) {
            LogE("GetInterface SL_IID_VOLUME fail! result = %d", result);
            return result;
        }
        /*
        LogE("start GetInterface SL_IID_VOLUME 2");
        result = (*p->outputMixObject)->GetInterface(p->outputMixObject, SL_IID_VOLUME, &p->bqPlayerVol2);
        if(result != SL_RESULT_SUCCESS) {
            LogE("GetInterface SL_IID_VOLUME fail! 2 result = %d", result);
            return result;
        }
		LogE("end GetInterface SL_IID_VOLUME 2 success");
		*/
		
        // register callback on the buffer queue
        result = (*p->bqPlayerBufferQueue)->RegisterCallback(p->bqPlayerBufferQueue, bqPlayerCallback, p);
        if(result != SL_RESULT_SUCCESS) {
            LogE("RegisterCallback fail! result = %d", result);
            return result;
        }

        // set the player's state to playing
        result = (*p->bqPlayerPlay)->SetPlayState(p->bqPlayerPlay, SL_PLAYSTATE_PLAYING);

		SLmillibel maxVol = -1;
		SLmillibel curVol = -1;
		result = (*p->bqPlayerVol)->GetMaxVolumeLevel(p->bqPlayerVol, &maxVol);
		if(result != SL_RESULT_SUCCESS) {
			LogE("GetMaxVolumeLevel fail! result = %d", result);
			return result;
		}

		result = (*p->bqPlayerVol)->SetVolumeLevel(p->bqPlayerVol, 0);
		if(result != SL_RESULT_SUCCESS) {
			LogE("SetVolumeLevel fail! result = %d", result);
			return result;
		}
		
		result = (*p->bqPlayerVol)->GetVolumeLevel(p->bqPlayerVol, &curVol);
		if(result != SL_RESULT_SUCCESS) {
			LogE("GetVolumeLevel fail! result = %d", result);
			return result;
		}
		LogE("openSLPlayOpen maxVol:%d curVol:%d", maxVol, curVol);
        /*
		result = (*p->bqPlayerVol2)->GetMaxVolumeLevel(p->bqPlayerVol2, &maxVol);
		if(result != SL_RESULT_SUCCESS) {
			LogE("GetMaxVolumeLevel fail! 2 result = %d", result);
			return result;
		}
		
		result = (*p->bqPlayerVol2)->GetVolumeLevel(p->bqPlayerVol2, &curVol);
		if(result != SL_RESULT_SUCCESS) {
			LogE("GetVolumeLevel fail! 2  result = %d", result);
			return result;
		}
		LogE("openSLPlayOpen 2 maxVol:%d curVol:%d", maxVol, curVol);
		*/
        return result;
    }else {
		LogD("openSLPlayOpen channels:%d", channels);
        return SL_RESULT_SUCCESS;
	}
}

// Open the OpenSL ES device for input
static SLresult openSLRecOpen(OPENSL_STREAM *p, int capStreamType)
{
    SLresult result;
    SLuint32 sr = p->sr;
    SLuint32 channels = p->inchannels;
	LogE("openSLRecOpen sr:%d channels:%d capStreamType:%d", sr, channels, capStreamType);

    if(channels) {
        switch (sr) {
            case 8000:
                sr = SL_SAMPLINGRATE_8;
                break;
            case 11025:
                sr = SL_SAMPLINGRATE_11_025;
                break;
            case 16000:
                sr = SL_SAMPLINGRATE_16;
                break;
            case 22050:
                sr = SL_SAMPLINGRATE_22_05;
                break;
            case 24000:
                sr = SL_SAMPLINGRATE_24;
                break;
            case 32000:
                sr = SL_SAMPLINGRATE_32;
                break;
            case 44100:
                sr = SL_SAMPLINGRATE_44_1;
                break;
            case 48000:
                sr = SL_SAMPLINGRATE_48;
                break;
            case 64000:
                sr = SL_SAMPLINGRATE_64;
                break;
            case 88200:
                sr = SL_SAMPLINGRATE_88_2;
                break;
            case 96000:
                sr = SL_SAMPLINGRATE_96;
                break;
            case 192000:
                sr = SL_SAMPLINGRATE_192;
                break;
            default:
                return -1;
        }

        // configure audio source
        SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
                                          SL_DEFAULTDEVICEID_AUDIOINPUT, NULL};
        SLDataSource audioSrc = {&loc_dev, NULL};

        // configure audio sink
        int speakers;
        if (channels > 1) {
            speakers = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
        } else {
            speakers = SL_SPEAKER_FRONT_CENTER;
        }

        SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
        SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, channels, sr,
                                       SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                       speakers, SL_BYTEORDER_LITTLEENDIAN};
        SLDataSink audioSnk = {&loc_bq, &format_pcm};

        // create audio recorder
        // (requires the RECORD_AUDIO permission)
        /*
        const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
        const SLboolean req[1] = {SL_BOOLEAN_TRUE};
        */
        const SLInterfaceID ids[2] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                      SL_IID_ANDROIDCONFIGURATION};
        const SLboolean req[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
        result = (*p->engineEngine)->CreateAudioRecorder(p->engineEngine, &(p->recorderObject), &audioSrc,
                                                         &audioSnk, 2, ids, req);
        if (SL_RESULT_SUCCESS != result) {
            LogD("CreateAudioRecorder fail! result = %d", result);
            return result;
        }

        /* begin; add by lideshou, 2017.12.11 */
        SLAndroidConfigurationItf recorderConfig;
        result = (*(p->recorderObject))->GetInterface(p->recorderObject,
                                                    SL_IID_ANDROIDCONFIGURATION,
                                                    &recorderConfig);
        if (result == SL_RESULT_SUCCESS) {
            LogE("Warning:get recorderConfig success");
            /* 15000 */
            //SLint32 streamType = SL_ANDROID_RECORDING_PRESET_VOICE_COMMUNICATION;
            /* strange */
            //SLint32 streamType = SL_ANDROID_RECORDING_PRESET_VOICE_RECOGNITION;
            /* big  normal */
            //SLint32 streamType = SL_ANDROID_RECORDING_PRESET_GENERIC;
            result = (*recorderConfig)->SetConfiguration(
                    recorderConfig, SL_ANDROID_KEY_RECORDING_PRESET,
                    &capStreamType, sizeof(SLint32));
        }
        if (result != SL_RESULT_SUCCESS) {
            LogE("Warning: Unable to set android recorder configuration");
        }
        /* end */

        // realize the audio recorder
        result = (*p->recorderObject)->Realize(p->recorderObject, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) {
            LogD("Realize fail! result = %d", result);
            return result;
        }

        // get the record interface
        result = (*p->recorderObject)->GetInterface(p->recorderObject, SL_IID_RECORD, &(p->recorderRecord));
        if (SL_RESULT_SUCCESS != result) {
            LogD("GetInterface SL_IID_RECORD fail! result = %d", result);
            return result;
        }

        // get the buffer queue interface
        result = (*p->recorderObject)->GetInterface(p->recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                    &(p->recorderBufferQueue));
        if (SL_RESULT_SUCCESS != result) {
            LogD("GetInterface SL_IID_ANDROIDSIMPLEBUFFERQUEUE fail! result = %d", result);
            return result;
        }

        // register callback on the buffer queue
        result = (*p->recorderBufferQueue)->RegisterCallback(p->recorderBufferQueue, bqRecorderCallback,
                                                             p);
        if (SL_RESULT_SUCCESS != result) {
            LogD("RegisterCallback fail! result = %d", result);
            return result;
        }

        result = (*p->recorderRecord)->SetRecordState(p->recorderRecord, SL_RECORDSTATE_RECORDING);

        return result;
    }
    else {
		LogD("openSLRecOpen channels = %d", channels);
        return SL_RESULT_SUCCESS;
	}
}

// close the OpenSL IO and destroy the audio engine
static void openSLDestroyEngine(OPENSL_STREAM *p)
{
    // destroy buffer queue audio player object, and invalidate all associated interfaces
    if (p->bqPlayerObject != NULL) {
        (*p->bqPlayerObject)->Destroy(p->bqPlayerObject);
        p->bqPlayerObject = NULL;
        p->bqPlayerPlay = NULL;
        p->bqPlayerBufferQueue = NULL;
        p->bqPlayerEffectSend = NULL;
    }

    // destroy audio recorder object, and invalidate all associated interfaces
    if (p->recorderObject != NULL) {
        (*p->recorderObject)->Destroy(p->recorderObject);
        p->recorderObject = NULL;
        p->recorderRecord = NULL;
        p->recorderBufferQueue = NULL;
    }

    // destroy output mix object, and invalidate all associated interfaces
    if (p->outputMixObject != NULL) {
        (*p->outputMixObject)->Destroy(p->outputMixObject);
        p->outputMixObject = NULL;
    }

    // destroy engine object, and invalidate all associated interfaces
    if (p->engineObject != NULL) {
        (*p->engineObject)->Destroy(p->engineObject);
        p->engineObject = NULL;
        p->engineEngine = NULL;
    }
}

// open the android audio device for input and/or output
/*
 * type: 0 is cap; 1 is play; other is cap and play */
extern "C" OPENSL_STREAM *android_OpenAudioDevice(int type, int sr, int capStreamType, int playStreamType, int inchannels, int outchannels, int bufferframes, bool isSco)
{    
    timespec time1 = { 0 };
    timespec time2 = { 0 };
    long timeLen = 0;
    char buf[64] = { 0 };
	
    OPENSL_STREAM *p;
    p = (OPENSL_STREAM *) calloc(sizeof(OPENSL_STREAM),1);

    p->inchannels = inchannels;
    p->outchannels = outchannels;
    p->sr = sr;
    p->inlock = createThreadLock();
    p->outlock = createThreadLock();

    LogD("android_OpenAudioDevice createThreadLock success; inlock:%p outlock:%p isSco:%d", p->inlock, p->outlock, isSco);

    if((p->outBufSamples = bufferframes * outchannels) != 0) {
        if((p->outputBuffer[0] = (short *) calloc(p->outBufSamples, sizeof(short))) == NULL ||
           (p->outputBuffer[1] = (short *) calloc(p->outBufSamples, sizeof(short))) == NULL) {
            android_CloseAudioDevice(p);
            return NULL;
        }
    }

    LogD("android_OpenAudioDevice p->outBufSamples:%d", p->outBufSamples);

    if((p->inBufSamples = bufferframes * inchannels) != 0){
        if((p->inputBuffer[0] = (short *) calloc(p->inBufSamples, sizeof(short))) == NULL ||
           (p->inputBuffer[1] = (short *) calloc(p->inBufSamples, sizeof(short))) == NULL){
            android_CloseAudioDevice(p);
            return NULL;
        }
    }

    LogD("android_OpenAudioDevice p->inBufSamples:%d", p->inBufSamples);

    p->currentInputIndex = 0;
    p->currentOutputBuffer  = 0;
    p->currentInputIndex = p->inBufSamples;
    p->currentInputBuffer = 0;
	
	LogE("android_OpenAudioDevice  **********************************");

	clock_gettime(CLOCK_REALTIME, &time1);
	
    if(openSLCreateEngine(p) != SL_RESULT_SUCCESS) {
        android_CloseAudioDevice(p);
        return NULL;
    }
    LogD("android_OpenAudioDevice openSLCreateEngine success");

    if (0 == type) {
        if (openSLRecOpen(p, capStreamType) != SL_RESULT_SUCCESS) {
            android_CloseAudioDevice(p);
            return NULL;
        }
		if (isSco) {
			LogE("android_OpenAudioDevice SCO cap, need open play");
			if (openSLPlayOpen(p, playStreamType) != SL_RESULT_SUCCESS) {
                android_CloseAudioDevice(p);
                return NULL;
            }
		}
        LogE("android_OpenAudioDevice openSLRecOpen success");
    }else if(1 == type) {
        if (openSLPlayOpen(p, playStreamType) != SL_RESULT_SUCCESS) {
            android_CloseAudioDevice(p);
            return NULL;
        }
        LogE("android_OpenAudioDevice openSLPlayOpen success");
    }else {
        if (openSLRecOpen(p, capStreamType) != SL_RESULT_SUCCESS) {
            android_CloseAudioDevice(p);
            return NULL;
        }
        if (openSLPlayOpen(p, playStreamType) != SL_RESULT_SUCCESS) {
            android_CloseAudioDevice(p);
            return NULL;
        }
        LogE("android_OpenAudioDevice open cap and play success");
    }

	clock_gettime(CLOCK_REALTIME, &time2);
	timeLen = (time2.tv_sec * 1000000 + time2.tv_nsec / 1000) - (time1.tv_sec * 1000000 + time1.tv_nsec / 1000);
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "%ld\n", timeLen);
	
	LogE("android_OpenAudioDevice  #######################################");

	if ((0 == type) && (g_pCapRecordTimeFile != NULL)) {
        if (fwrite((unsigned char *)buf , strlen(buf), 1, g_pCapRecordTimeFile) != 1) {
            LogE("android_OpenAudioDevice  failed to save record cap time!");
        }		
	}else if ((1 == type) && (g_pPlayRecordTimeFile != NULL)) {	
		if (fwrite((unsigned char *)buf , strlen(buf), 1, g_pPlayRecordTimeFile) != 1) {
			LogE("android_OpenAudioDevice  failed to save record play time!");
		}
	}

    notifyThreadLock(p->outlock);
    notifyThreadLock(p->inlock);

    p->time = 0.0;
	LogD("android_OpenAudioDevice success");
    return p;
}

// close the android audio device
extern "C" void android_CloseAudioDevice(OPENSL_STREAM *p)
{
    if (p == NULL)
        return;

    openSLDestroyEngine(p);

    if (p->inlock != NULL) {
        notifyThreadLock(p->inlock);
        destroyThreadLock(p->inlock);
        p->inlock = NULL;
    }

    if (p->outlock != NULL) {
        notifyThreadLock(p->outlock);
        destroyThreadLock(p->outlock);
        p->inlock = NULL;
    }

    if (p->outputBuffer[0] != NULL) {
        free(p->outputBuffer[0]);
        p->outputBuffer[0] = NULL;
    }

    if (p->outputBuffer[1] != NULL) {
        free(p->outputBuffer[1]);
        p->outputBuffer[1] = NULL;
    }

    if (p->inputBuffer[0] != NULL) {
        free(p->inputBuffer[0]);
        p->inputBuffer[0] = NULL;
    }

    if (p->inputBuffer[1] != NULL) {
        free(p->inputBuffer[1]);
        p->inputBuffer[1] = NULL;
    }

    free(p);
}

// returns timestamp of the processed stream
extern "C" double android_GetTimestamp(OPENSL_STREAM *p)
{
    return p->time;
}

// this callback handler is called every time a buffer finishes recording
void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    OPENSL_STREAM *p = (OPENSL_STREAM *) context;
	//LogE("bqRecorderCallback currentInputIndex:%d currentOutputIndex:%d", p->currentInputIndex, p->currentOutputIndex);
    notifyThreadLock(p->inlock);
}

// gets a buffer of size samples from the device
extern "C" int android_AudioIn(OPENSL_STREAM *p,short *buffer,int size)
{
    short *inBuffer;
    int i, bufsamps = p->inBufSamples, index = p->currentInputIndex;
    if(p == NULL || bufsamps ==  0) {
        LogE("android_AudioIn do nothing; p:%p bufsamps:%d", p, bufsamps);
        return 0;
    }

    //LogD("android_AudioIn bufsamps:%d index:%d currentInputBuffer:%d size:%d", bufsamps, index, p->currentInputBuffer, size);
    inBuffer = p->inputBuffer[p->currentInputBuffer];
    for(i = 0; i < size; i++){
        if (index >= bufsamps) {
            //usleep(20);
            //LogD("android_AudioIn inBuffer:%p", inBuffer);
            waitThreadLock(p->inlock);
            //LogD("android_AudioIn inBuffer:%p bufsamps:%d recorderBufferQueue:%p %p", inBuffer, bufsamps, p->recorderBufferQueue, *p->recorderBufferQueue);
            (*p->recorderBufferQueue)->Enqueue(p->recorderBufferQueue, inBuffer, bufsamps * sizeof(short));
            p->currentInputBuffer = (p->currentInputBuffer ? 0 : 1);
            index = 0;
            inBuffer = p->inputBuffer[p->currentInputBuffer];
        }
        buffer[i] = (short)inBuffer[index++];
    }
    LogD("android_AudioIn index:%d", index);
    p->currentInputIndex = index;
    if(p->outchannels == 0) p->time += (double) size/(p->sr * p->inchannels);
    return i;
}

// this callback handler is called every time a buffer finishes playing
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    OPENSL_STREAM *p = (OPENSL_STREAM *) context;
	//LogE("bqPlayerCallback currentInputIndex:%d currentOutputIndex:%d", p->currentInputIndex, p->currentOutputIndex);
    notifyThreadLock(p->outlock);
}

// puts a buffer of size samples to the device
extern "C" int android_AudioOut(OPENSL_STREAM *p, short *buffer, int size)
{	
    short *outBuffer;
    int i, bufsamps = p->outBufSamples, index = p->currentOutputIndex;
    if(p == NULL  || bufsamps ==  0)  {
        LogD("android_AudioOut do nothing");
        return 0;
    }
    //LogD("android_AudioOut bufsamps:%d size:%d p->outBufSamples:%d p->currentOutputBuffer:%d", bufsamps, size, p->outBufSamples, p->currentOutputBuffer);
    outBuffer = p->outputBuffer[p->currentOutputBuffer];

    for(i = 0; i < size; i++){
        outBuffer[index++] = (short)(buffer[i]);
        if (index >= p->outBufSamples) {
            waitThreadLock(p->outlock);
            (*p->bqPlayerBufferQueue)->Enqueue(p->bqPlayerBufferQueue,
                                               outBuffer, bufsamps * sizeof(short));
            p->currentOutputBuffer = (p->currentOutputBuffer ?  0 : 1);
            index = 0;
            outBuffer = p->outputBuffer[p->currentOutputBuffer];
        }
    }
    p->currentOutputIndex = index;
    p->time += (double) size/(p->sr*p->outchannels);
    return i;
}

//----------------------------------------------------------------------
// thread Locks
// to ensure synchronisation between callbacks and processing code
void* createThreadLock(void)
{
    threadLock  *p;
    p = (threadLock*) malloc(sizeof(threadLock));
    if (p == NULL) {
		LogE("createThreadLock malloc fail");
        return NULL;
	}
    memset(p, 0, sizeof(threadLock));
    if (pthread_mutex_init(&(p->m), (pthread_mutexattr_t*) NULL) != 0) {
        free((void*) p);
		LogE("createThreadLock pthread_mutex_init fail");
        return NULL;
    }
    if (pthread_cond_init(&(p->c), (pthread_condattr_t*) NULL) != 0) {
        pthread_mutex_destroy(&(p->m));
        free((void*) p);
		LogE("createThreadLock pthread_cond_init fail");
        return NULL;
    }
    p->s = (unsigned char)1;

    return p;
}

void waitThreadLock(void *lock)
{
    threadLock  *p;
    int retval = 0;
    p = (threadLock*)lock;
    //LogD("waitThreadLock enter p:%p", p);
    pthread_mutex_lock(&(p->m));
    while (!p->s) {
        pthread_cond_wait(&(p->c), &(p->m));
    }
    p->s = (unsigned char)0;
    pthread_mutex_unlock(&(p->m));
    //LogD("waitThreadLock end");
}

void notifyThreadLock(void *lock)
{
    threadLock *p;
    p = (threadLock*) lock;
	//LogD("notifyThreadLock enter p:%p", p);
    pthread_mutex_lock(&(p->m));
    p->s = (unsigned char)1;
    pthread_cond_signal(&(p->c));
    pthread_mutex_unlock(&(p->m));
	//LogD("notifyThreadLock end");
}

void destroyThreadLock(void *lock)
{
    threadLock  *p;
    p = (threadLock*) lock;
    if (p == NULL) {
		LogD("destroyThreadLock p == NULL");
        return;
	}
    notifyThreadLock(p);
    pthread_cond_destroy(&(p->c));
    pthread_mutex_destroy(&(p->m));
    free(p);
}

int startCap(const char * pFileName, char * pMode, int chanNum, int capStreamType, int playStreamType, int sampleRate, int frameSize, int frameNum, bool isSco) {
    LogE("startCap name:%s mode:%s chanNum:%d capStreamType:%d playStreamType:%d sampleRate:%d frameSize:%d frameNum:%d isSco:%d", pFileName, pMode, chanNum, capStreamType, playStreamType, sampleRate, frameSize, frameNum, isSco);
    FILE * fp = fopen(pFileName, pMode);
    if( fp == NULL ) {
        LogE("cannot open file (%s)", pFileName);
        return -1;
    }
    //LogD("startCap open file success");
    OPENSL_STREAM* stream = android_OpenAudioDevice(0, sampleRate, capStreamType, playStreamType, chanNum, chanNum, frameSize, isSco);
    if (stream == NULL) {
        fclose(fp);
        LogE("failed to open audio device!");
        return -2;
    }
    //LogD("startCap open audio device success");
    int samples = 0;
    int num = 0;
	int bufferSize = frameSize * 2;
    short buffer[bufferSize];
	short muteBuffer[bufferSize];
	memset(buffer, 0, sizeof(short) * bufferSize);
	memset(muteBuffer, 0, sizeof(short) * bufferSize);
	int shortSize = sizeof(short);
	int tmp = 0;
    int j = 0;
    g_loop_exit = 0;
    while ((!g_loop_exit) && (num++ < frameNum)) {
        //LogD("startCap start loop; num:%d frameNum:%d", num, frameNum);
        samples = android_AudioIn(stream, buffer, bufferSize);
        if (samples < 0) {
            LogE("android_AudioIn failed!");
            break;
        }
        //LogE("startCap get *** cap data success; num:%d frameNum:%d samples:%d", num, frameNum, samples);
        if (fwrite((unsigned char *)buffer, samples * sizeof(short), 1, fp) != 1) {
            LogE("failed to save captured data!");
            break;
        }
        if (isSco) {
            samples = android_AudioOut(stream, muteBuffer, bufferSize);
            if (samples < 0) {
                LogE("android_AudioOut failed!");
            }
        }
        //LogD("capture %d samples  num = %d frameNum = %d", samples, num, frameNum);
    }

    android_CloseAudioDevice(stream);
    fclose(fp);

    LogD("startCap completed!");
	return 0;
}

int startPlay(const char * pFileName, char * pMode, int playStreamType, int sampleRate, int frameSize, int frameNum) {
    LogE("startPlay name:%s mode:%s playStreamType:%d sampleRate:%d frameSize:%d frameNum:%d", pFileName, pMode, playStreamType, sampleRate, frameSize, frameNum);
    FILE *fp = fopen(pFileName, pMode);
    if (fp == NULL) {
        LogE("cannot open file (%s)!", pFileName);
        return -1;
    }
    //LogD("startPlay open file success");
    OPENSL_STREAM *stream = android_OpenAudioDevice(1, sampleRate, -1, playStreamType, 1, 1, frameSize, false);
    if (stream == NULL) {
        fclose(fp);
        LogE("failed to open audio device!");
        return -2;
    }
    //LogD("startPlay open audio device success");
    int samples = 0;
    int num = 0;
	int bufferSize = frameSize * 1;
    short buffer[bufferSize];
	memset(buffer, 0, sizeof(short) * bufferSize);
    g_loop_exit = 0;
    int shortSize = sizeof(short);
    while (!g_loop_exit && (num++ < frameNum)) {
        //LogE("startPlay *** start loop; num:%d frameNum:%d", num, frameNum);
        if (fread((unsigned char *) buffer, bufferSize * shortSize, 1, fp) != 1) {
            LogE("failed to read data");
            break;
        }
        //LogD("startPlay read data success");
        samples = android_AudioOut(stream, buffer, bufferSize);
        if (samples < 0) {
            LogE("android_AudioOut failed!");
        }
        //LogD("play %d samples! num = %d frameNum = %d", samples, num, frameNum);
    }

    android_CloseAudioDevice(stream);
    fclose(fp);

    LogD("StartPlay completed!");
	return 0;
}

int startLoop(int chanNum, int capStreamType, int playStreamType, int sampleRate, int frameSize, int frameNum) {
    LogE("startLoop chanNum:%d capStreamType:%d playStreamType:%d sampleRate:%d frameSize:%d frameNum:%d", chanNum, capStreamType, playStreamType, sampleRate, frameSize, frameNum);
    OPENSL_STREAM* stream = android_OpenAudioDevice(2, sampleRate, capStreamType, playStreamType, chanNum, chanNum, frameSize, false);
    if (stream == NULL) {
        LogE("startLoop failed to open audio device!");
        return -2;
    }
#if 0
    FILE *fpPlay = fopen("/sdcard/play16000stereo.pcm", "rb");
    if (fpPlay == NULL) {
        LogE("cannot open file (%s)!", "/sdcard/play16000stereo.pcm");
        return -1;
    }
    FILE *fpRec = fopen("/sdcard/rec16000stereo.pcm", "wb");
    if (fpRec == NULL) {
        LogE("cannot open file (%s)!", "/sdcard/rec16000stereo.pcm");
        return -1;
    }
#endif
    //LogD("startLoop open audio device success");
    int samples = 0;
    int num = 0;
    int bufferSize = frameSize * 2;
    short buffer[bufferSize];
    memset(buffer, 0, sizeof(short) * bufferSize);
#if 0
    short buffer2[bufferSize];
	memset(buffer2, 0, sizeof(short) * bufferSize);
#endif
    int shortSize = sizeof(short);
    short tmp = 0;
    short tmp2 = 0;
    short tmp3 = 0;
    int j = 0;
    g_loop_exit = 0;
    while ((!g_loop_exit) && (num++ < frameNum)) {
#if 1
        //LogD("startLoop start loop; num:%d frameNum:%d", num, frameNum);
        samples = android_AudioIn(stream, buffer, bufferSize);
        if (samples < 0) {
            LogE("startLoop android_AudioIn failed!");
            break;
        }
        //LogE("startLoop get *** cap data success; num:%d frameNum:%d samples:%d", num, frameNum, samples);
        //memset(buffer, 0, bufferSize * shortSize);

        tmp = 0;
        for (j = 0; j < bufferSize; j++) {
            tmp = buffer[j];
            //tmp = tmp / 32;
            tmp = tmp / 16;
            //tmp = tmp / 8;
            //buffer[j] = (short)tmp;
            //tmp2 = ntohs(tmp);
            tmp2 = htons(tmp);
            tmp3 = htons(tmp2);
            buffer[j] = (short)tmp3;
        }
        samples = android_AudioOut(stream, buffer, bufferSize);
        if (samples < 0) {
            LogE("startLoop android_AudioOut failed!");
        }
#else
        if (fread((unsigned char *) buffer2, bufferSize * shortSize, 1, fpPlay) != 1) {
            LogE("failed to read data");
            break;
        }
        //LogD("startPlay read data success");
        samples = android_AudioOut(stream, buffer2, bufferSize);
        if (samples < 0) {
            LogE("startLoop android_AudioOut failed!");
        }

        samples = android_AudioIn(stream, buffer, bufferSize);
        if (samples < 0) {
            LogE("startLoop android_AudioIn failed!");
            break;
        }
        if (fwrite((unsigned char *)buffer, samples * sizeof(short), 1, fpRec) != 1) {
            LogE("failed to save captured data!");
            break;
        }
#endif
        //LogD("startLoop capture %d samples  num = %d frameNum = %d", samples, num, frameNum);
    }
    android_CloseAudioDevice(stream);
#if 0
    fclose(fpPlay);
    fclose(fpRec);
#endif
    LogD("startLoop completed!");
    return 0;
}
#if 0
extern "C" JNIEXPORT jint JNICALL Java_com_hytera_openslesdemo_NativeAudio_startCap(JNIEnv *env, jobject thiz, jstring fileName, jint capSource, jint sampleRate, jboolean isStressTest, jint stressTestNum) {
    const char *pRecordFile = env->GetStringUTFChars(fileName, NULL);
    if (pRecordFile == NULL) {
        LogE("get filename fail");
        return -1;
    }
    int frameSize = (sampleRate * 20) / 1000;
    int ret = 0;
    g_stress_exit = 0;
    LogD("recordFile:%s capSource:%d sampleRate:%d isStressTest:%d stressTestNum:%d", pRecordFile, capSource, sampleRate, isStressTest, stressTestNum);
    if (isStressTest) {
        for (int i = 0; i < stressTestNum && (g_stress_exit == 0); i++) {
            /* stress test a cap 1s data */
            ret = startCap(pRecordFile, "ab", capSource, sampleRate, frameSize, 50);
            if (ret < 0) {
                LogE("cap stress test fail! ret = %d", ret);
                break;
            }
        }
    }else {
        /* stress test a cap max long 120s data */
        ret = startCap(pRecordFile, "wb", capSource, sampleRate,frameSize, 6000);
        if (ret < 0) {
            LogE("startCap fail! ret = %d", ret);
        }
    }
    env->ReleaseStringUTFChars(fileName, pRecordFile);
    return ret;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hytera_openslesdemo_NativeAudio_stopCap(JNIEnv *env, jobject thiz) {
    LogD("stop cap");
    g_loop_exit = 1;
    g_stress_exit = 1;
    return 0;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hytera_openslesdemo_NativeAudio_startPlay(JNIEnv *env, jobject thiz, jstring fileName, jint playSource, jint sampleRate, jboolean isStressTest, jint stressTestNum) {
    const char *pPlayFile = env->GetStringUTFChars(fileName, NULL);
    if (pPlayFile == NULL) {
        LogE("get playFileName fail");
        return -1;
    }
    int frameSize = (sampleRate * 20) / 1000;
    int ret = 0;
    g_stress_exit = 0;
    LogD("playFile:%s playSource:%d sampleRate:%d isStressTest:%d stressTestNum:%d", pPlayFile, playSource, sampleRate, isStressTest, stressTestNum);
    if (isStressTest) {
        for (int i = 0; i < stressTestNum && g_stress_exit == 0; i++) {
            /* stress test a play 1s data */
            ret = startPlay(pPlayFile, "rb", playSource, sampleRate, frameSize, 50);
            if (ret < 0) {
                LogE("play stress test fail! ret = %d", ret);
                break;
            }
        }
    }else {
        /* stress test a play 180s data */
        ret = startPlay(pPlayFile, "rb", playSource, sampleRate,frameSize, 9000);
        if (ret < 0) {
            LogE("startPlay fail! ret = %d", ret);
        }
    }
    env->ReleaseStringUTFChars(fileName, pPlayFile);
    return ret;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hytera_openslesdemo_NativeAudio_stopPlay(JNIEnv *env, jobject thiz) {
    LogD("stop play");
    g_loop_exit = 1;
    g_stress_exit = 1;
    return 0;
}
#endif
extern "C" JNIEXPORT jint JNICALL startCapJni(JNIEnv *env, jobject thiz, jstring fileName, jstring recordTimeFileName, jint chanNum, jint capStreamType, jint playStreamType, jint sampleRate, jboolean isStressTest, jint stressTestNum, jboolean isSco) {
    const char *pRecordFile = env->GetStringUTFChars(fileName, NULL);
    if (pRecordFile == NULL) {
        LogE("get filename fail");
        return -1;
    }
	
    const char *pRecordTimeFile = env->GetStringUTFChars(recordTimeFileName, NULL);
    if (pRecordTimeFile == NULL) {
        LogE("get record time filename fail");
		env->ReleaseStringUTFChars(fileName, pRecordFile);
        return -1;
    }
	
    if ((NULL == g_pCapRecordTimeFile) && isStressTest) {
		g_pCapRecordTimeFile = fopen(pRecordTimeFile, "wb");
		if(g_pCapRecordTimeFile == NULL) {
			LogE("cannot open record cap time file (%s)", pRecordTimeFile);
		}
    }
	
    int frameSize = (sampleRate * 20) / 1000;
    int ret = 0;
    g_stress_exit = 0;
    LogE("recordFile:%s pRecordTimeFile:%s chanNum:%d capStreamType:%d playStreamType:%d sampleRate:%d isStressTest:%d stressTestNum:%d isSco:%d", pRecordFile, pRecordTimeFile, chanNum, capStreamType, playStreamType, sampleRate, isStressTest, stressTestNum, isSco);
    if (isStressTest) {
        for (int i = 0; i < stressTestNum && (g_stress_exit == 0); i++) {
            /* stress test a cap 1s data */
            ret = startCap(pRecordFile, "ab", chanNum, capStreamType, playStreamType, sampleRate, frameSize, 50, isSco);
            if (ret < 0) {
                LogE("cap stress test fail! ret = %d", ret);
                break;
            }
        }
    }else {
        /* stress test a cap max long 120s data */
        ret = startCap(pRecordFile, "wb", chanNum, capStreamType, playStreamType, sampleRate,frameSize, 6000, isSco);
        if (ret < 0) {
            LogE("startCap fail! ret = %d", ret);
        }
    }
    env->ReleaseStringUTFChars(fileName, pRecordFile);
	env->ReleaseStringUTFChars(recordTimeFileName, pRecordTimeFile);

	if (g_pCapRecordTimeFile != NULL) {
		fclose(g_pCapRecordTimeFile);
		g_pCapRecordTimeFile = NULL;
	}
    return ret;
}

extern "C" JNIEXPORT jint JNICALL stopCapJni(JNIEnv *env, jobject thiz) {
    LogD("stop cap");
    g_loop_exit = 1;
    g_stress_exit = 1;
    return 0;
}

extern "C" JNIEXPORT jint JNICALL startPlayJni(JNIEnv *env, jobject thiz, jstring fileName, jstring recordTimeFileName, jint playStreamType, jint sampleRate, jboolean isStressTest, jint stressTestNum) {
    const char *pPlayFile = env->GetStringUTFChars(fileName, NULL);
    if (pPlayFile == NULL) {
        LogE("get playFileName fail");
        return -1;
    }

    const char *pRecordTimeFile = env->GetStringUTFChars(recordTimeFileName, NULL);
    if (pRecordTimeFile == NULL) {
        LogE("get record time filename fail");
		env->ReleaseStringUTFChars(fileName, pPlayFile);
        return -1;
    }
	
	if ((NULL == g_pPlayRecordTimeFile) && isStressTest) {
		g_pPlayRecordTimeFile = fopen(pRecordTimeFile, "wb");
		if(g_pPlayRecordTimeFile == NULL) {
			LogE("cannot open record play time file (%s)", pRecordTimeFile);
		}
	}

    int frameSize = (sampleRate * 20) / 1000;
    int ret = 0;
    g_stress_exit = 0;
    LogE("playFile:%s pRecordTimeFile:%s playStreamType:%d sampleRate:%d isStressTest:%d stressTestNum:%d", pPlayFile, pRecordTimeFile, playStreamType, sampleRate, isStressTest, stressTestNum);
    if (isStressTest) {
        for (int i = 0; i < stressTestNum && g_stress_exit == 0; i++) {
            /* stress test a play 1s data */
            ret = startPlay(pPlayFile, "rb", playStreamType, sampleRate, frameSize, 50);
            if (ret < 0) {
                LogE("play stress test fail! ret = %d", ret);
                break;
            }
        }
    }else {
        /* stress test a play 180s data */
        ret = startPlay(pPlayFile, "rb", playStreamType, sampleRate,frameSize, 9000);
        if (ret < 0) {
            LogE("startPlay fail! ret = %d", ret);
        }
    }
    env->ReleaseStringUTFChars(fileName, pPlayFile);
	env->ReleaseStringUTFChars(recordTimeFileName, pRecordTimeFile);

	if (g_pPlayRecordTimeFile != NULL) {
		fclose(g_pPlayRecordTimeFile);
		g_pPlayRecordTimeFile = NULL;
	}
    return ret;
}

extern "C" JNIEXPORT jint JNICALL stopPlayJni(JNIEnv *env, jobject thiz) {
    LogD("stop play");
    g_loop_exit = 1;
    g_stress_exit = 1;
    return 0;
}

extern "C" JNIEXPORT jint JNICALL startLoopJni(JNIEnv *env, jobject thiz, jint chanNum, jint capStreamType, jint playStreamType, jint sampleRate) {
    int frameSize = (sampleRate * 20) / 1000;
    int ret = 0;
    LogE("chanNum:%d capStreamType:%d playStreamType:%d sampleRate:%d", chanNum, capStreamType, playStreamType, sampleRate);
    ret = startLoop(chanNum, capStreamType, playStreamType, sampleRate, frameSize, 9000);
    if (ret < 0) {
        LogE("startLoop fail! ret = %d", ret);
    }
    return ret;
}

extern "C" JNIEXPORT jint JNICALL stopLoopJni(JNIEnv *env, jobject thiz) {
    LogD("stop loop");
    g_loop_exit = 1;
    return 0;
}

static JNINativeMethod Audio_jni_methods[] = {
        {"startCap", "(Ljava/lang/String;Ljava/lang/String;IIIIZIZ)I", (void *)startCapJni},
        {"stopCap", "()I", (void *)stopCapJni},
        {"startPlay", "(Ljava/lang/String;Ljava/lang/String;IIZI)I", (void *)startPlayJni},
        {"stopPlay", "()I", (void *)stopPlayJni},
        {"startLoop", "(IIII)I", (void *)startLoopJni},
        {"stopLoop", "()I", (void *)stopLoopJni}
};

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env = NULL;
    jclass clazz;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        LogE("GetEnv failed\n");
        return JNI_ERR;
    }

    clazz = env->FindClass(NATIVE_JNI_CLASS_PATH_NAME);
    if (clazz == NULL) {
        LogE("Native registration unable to find class '%s'", NATIVE_JNI_CLASS_PATH_NAME);
        return JNI_ERR;
    }

    if(env->RegisterNatives(clazz, Audio_jni_methods, METHOD_COUNT(Audio_jni_methods)) < 0) {
        LogE("native registration failed");
        return JNI_ERR;
    }

    return JNI_VERSION_1_4;
}
