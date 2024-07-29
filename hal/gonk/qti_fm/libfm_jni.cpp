/*
 * Copyright (c) 2009-2015, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *        * Redistributions of source code must retain the above copyright
 *            notice, this list of conditions and the following disclaimer.
 *        * Redistributions in binary form must reproduce the above copyright
 *            notice, this list of conditions and the following disclaimer in the
 *            documentation and/or other materials provided with the distribution.
 *        * Neither the name of The Linux Foundation nor
 *            the names of its contributors may be used to endorse or promote
 *            products derived from this software without specific prior written
 *            permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.    IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "android_hardware_fm"

#include <jni.h>
#include <nativehelper/JNIHelp.h>
#include <utils/Log.h>
#include <utils/misc.h>
#include <cutils/properties.h>
#include <fcntl.h>
#include <math.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <assert.h>
#include <dlfcn.h>
#include <android_runtime/AndroidRuntime.h>
#include <android_runtime/Log.h>
#include "bt_configstore.h"
#include <vector>
#include "radio-helium.h"

typedef  unsigned int UINT;
typedef  unsigned long ULINT;
#define STD_BUF_SIZE  256
#define FM_JNI_SUCCESS 0L
#define FM_JNI_FAILURE -1L
#define SEARCH_DOWN 0
#define SEARCH_UP 1
#define HIGH_BAND 2
#define LOW_BAND  1
#define CAL_DATA_SIZE 23
#define V4L2_CTRL_CLASS_USER 0x00980000
#define V4L2_CID_PRIVATE_IRIS_SET_CALIBRATION           (V4L2_CTRL_CLASS_USER + 0x92A)
#define V4L2_CID_PRIVATE_TAVARUA_ON_CHANNEL_THRESHOLD   (V4L2_CTRL_CLASS_USER + 0x92B)
#define V4L2_CID_PRIVATE_TAVARUA_OFF_CHANNEL_THRESHOLD  (V4L2_CTRL_CLASS_USER + 0x92C)
#define V4L2_CID_PRIVATE_IRIS_SET_SPURTABLE             (V4L2_CTRL_CLASS_USER + 0x92D)
#define TX_RT_LENGTH       63
#define WAIT_TIMEOUT 200000 /* 200*1000us */
#define TX_RT_DELIMITER    0x0d
#define PS_LEN    9
#define V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_RT 0x08000017
#define V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_PS_NAME 0x08000016
#define V4L2_CID_PRIVATE_UPDATE_SPUR_TABLE 0x08000034
#define V4L2_CID_PRIVATE_TAVARUA_TX_SETPSREPEATCOUNT 0x08000034
#define MASK_PI                    (0x0000FFFF)
#define MASK_PI_MSB                (0x0000FF00)
#define MASK_PI_LSB                (0x000000FF)
#define MASK_PTY                   (0x0000001F)
#define MASK_TXREPCOUNT            (0x0000000F)

enum FM_V4L2_PRV_CONTROLS
{
    V4L2_CID_PRV_BASE = 0x8000000,
    V4L2_CID_PRV_SRCHMODE,
    V4L2_CID_PRV_SCANDWELL,
    V4L2_CID_PRV_SRCHON,
    V4L2_CID_PRV_STATE,
    V4L2_CID_PRV_TRANSMIT_MODE,
    V4L2_CID_PRV_RDSGROUP_MASK,
    V4L2_CID_PRV_REGION,
    V4L2_CID_PRV_SIGNAL_TH,
    V4L2_CID_PRV_SRCH_PTY,
    V4L2_CID_PRV_SRCH_PI,
    V4L2_CID_PRV_SRCH_CNT,
    V4L2_CID_PRV_EMPHASIS,
    V4L2_CID_PRV_RDS_STD,
    V4L2_CID_PRV_CHAN_SPACING,
    V4L2_CID_PRV_RDSON,
    V4L2_CID_PRV_RDSGROUP_PROC,
    V4L2_CID_PRV_LP_MODE,
    V4L2_CID_PRV_INTDET = V4L2_CID_PRV_BASE + 25,
    V4L2_CID_PRV_AF_JUMP = V4L2_CID_PRV_BASE + 27,
    V4L2_CID_PRV_SOFT_MUTE = V4L2_CID_PRV_BASE + 30,
    V4L2_CID_PRV_AUDIO_PATH = V4L2_CID_PRV_BASE + 41,
    V4L2_CID_PRV_SINR = V4L2_CID_PRV_BASE + 44,
    V4L2_CID_PRV_ON_CHANNEL_THRESHOLD = V4L2_CID_PRV_BASE + 0x2D,
    V4L2_CID_PRV_OFF_CHANNEL_THRESHOLD,
    V4L2_CID_PRV_SINR_THRESHOLD,
    V4L2_CID_PRV_SINR_SAMPLES,
    V4L2_CID_PRV_SPUR_FREQ,
    V4L2_CID_PRV_SPUR_FREQ_RMSSI,
    V4L2_CID_PRV_SPUR_SELECTION,
    V4L2_CID_PRV_AF_RMSSI_TH = V4L2_CID_PRV_BASE + 0x36,
    V4L2_CID_PRV_AF_RMSSI_SAMPLES,
    V4L2_CID_PRV_GOOD_CH_RMSSI_TH,
    V4L2_CID_PRV_SRCHALGOTYPE,
    V4L2_CID_PRV_CF0TH12,
    V4L2_CID_PRV_SINRFIRSTSTAGE,
    V4L2_CID_PRV_RMSSIFIRSTSTAGE,
    V4L2_CID_PRV_SOFT_MUTE_TH,
    V4L2_CID_PRV_IRIS_RDSGRP_RT,
    V4L2_CID_PRV_IRIS_RDSGRP_PS_SIMPLE,
    V4L2_CID_PRV_IRIS_RDSGRP_AFLIST,
    V4L2_CID_PRV_IRIS_RDSGRP_ERT,
    V4L2_CID_PRV_IRIS_RDSGRP_RT_PLUS,
    V4L2_CID_PRV_IRIS_RDSGRP_3A,

    V4L2_CID_PRV_IRIS_READ_DEFAULT = V4L2_CTRL_CLASS_USER + 0x928,
    V4L2_CID_PRV_IRIS_WRITE_DEFAULT,
    V4L2_CID_PRV_SET_CALIBRATION = V4L2_CTRL_CLASS_USER + 0x92A,
    HCI_FM_HELIUM_SET_SPURTABLE = 0x0098092D,
    HCI_FM_HELIUM_GET_SPUR_TBL  = 0x0098092E,
    V4L2_CID_PRV_IRIS_FREQ,
    V4L2_CID_PRV_IRIS_SEEK,
    V4L2_CID_PRV_IRIS_UPPER_BAND,
    V4L2_CID_PRV_IRIS_LOWER_BAND,
    V4L2_CID_PRV_IRIS_AUDIO_MODE,
    V4L2_CID_PRV_IRIS_RMSSI,

    V4L2_CID_PRV_ENABLE_SLIMBUS = 0x00980940,
};

enum search_dir_t {
    SEEK_UP,
    SEEK_DN,
    SCAN_UP,
    SCAN_DN
};

enum fm_prop_t {
    FMWAN_RATCONF,
    FMBTWLAN_LPFENABLER
};

static JavaVM *g_jVM = NULL;

namespace android {

char *FM_LIBRARY_NAME = "fm_helium.so";
char *FM_LIBRARY_SYMBOL_NAME = "FM_HELIUM_LIB_INTERFACE";
void *lib_handle;
static int slimbus_flag = 0;

static char soc_name[16];
bool isSocNameAvailable = false;
static bt_configstore_interface_t* bt_configstore_intf = NULL;
static void *bt_configstore_lib_handle = NULL;

static JNIEnv *mCallbackEnv = NULL;
static jobject mCallbacksObj = NULL;
static bool mCallbacksObjCreated = false;

static jclass javaClassRef;
static jmethodID method_psInfoCallback;
static jmethodID method_rtCallback;
static jmethodID method_ertCallback;
static jmethodID method_aflistCallback;
static jmethodID method_rtplusCallback;
static jmethodID method_eccCallback;

jmethodID method_enableCallback;
jmethodID method_tuneCallback;
jmethodID method_seekCmplCallback;
jmethodID method_scanNxtCallback;
jmethodID method_srchListCallback;
jmethodID method_stereostsCallback;
jmethodID method_rdsAvlStsCallback;
jmethodID method_disableCallback;
jmethodID method_getSigThCallback;
jmethodID method_getChDetThrCallback;
jmethodID method_defDataRdCallback;
jmethodID method_getBlendCallback;
jmethodID method_setChDetThrCallback;
jmethodID method_defDataWrtCallback;
jmethodID method_setBlendCallback;
jmethodID method_getStnParamCallback;
jmethodID method_getStnDbgParamCallback;
jmethodID method_enableSlimbusCallback;
jmethodID method_enableSoftMuteCallback;
jmethodID method_FmReceiverJNICtor;

int load_bt_configstore_lib();

static bool checkCallbackThread() {
   JNIEnv* env = AndroidRuntime::getJNIEnv();
   if (mCallbackEnv != env || mCallbackEnv == NULL)
   {
       ALOGE("Callback env check fail: env: %p, callback: %p", env, mCallbackEnv);
       return false;
   }
    return true;
}

void fm_enabled_cb(void) {
    ALOGD("Entered %s", __func__);

    if (slimbus_flag) {
        if (!checkCallbackThread())
            return;

        mCallbackEnv->CallVoidMethod(mCallbacksObj, method_enableCallback);
    } else {
        if (mCallbackEnv != NULL) {
            ALOGE("javaObjectRef creating");
            jobject javaObjectRef =  mCallbackEnv->NewObject(javaClassRef, method_FmReceiverJNICtor);
            mCallbacksObj = javaObjectRef;
            ALOGE("javaObjectRef = %p mCallbackobject =%p \n",javaObjectRef,mCallbacksObj);
            mCallbackEnv->CallVoidMethod(mCallbacksObj, method_enableCallback);
        }
    }
    ALOGD("exit  %s", __func__);
}

void fm_tune_cb(int Freq)
{
    ALOGD("TUNE:Freq:%d", Freq);
    if (!checkCallbackThread())
        return;

    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_tuneCallback, (jint) Freq);
}

void fm_seek_cmpl_cb(int Freq)
{
    ALOGI("SEEK_CMPL: Freq: %d", Freq);
    if (!checkCallbackThread())
        return;

    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_seekCmplCallback, (jint) Freq);
}

void fm_scan_next_cb(void)
{
    ALOGI("SCAN_NEXT");
    if (!checkCallbackThread())
        return;

    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_scanNxtCallback);
}

void fm_srch_list_cb(uint16_t *scan_tbl)
{
    ALOGI("SRCH_LIST");
    jbyteArray srch_buffer = NULL;

    if (!checkCallbackThread())
        return;

    srch_buffer = mCallbackEnv->NewByteArray(STD_BUF_SIZE);
    if (srch_buffer == NULL) {
        ALOGE(" af list allocate failed :");
        return;
    }
    mCallbackEnv->SetByteArrayRegion(srch_buffer, 0, STD_BUF_SIZE, (jbyte *)scan_tbl);
    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_srchListCallback, srch_buffer);
    mCallbackEnv->DeleteLocalRef(srch_buffer);
}

void fm_stereo_status_cb(bool stereo)
{
    ALOGI("STEREO: %d", stereo);
    if (!checkCallbackThread())
        return;

    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_stereostsCallback, (jboolean) stereo);
}

void fm_rds_avail_status_cb(bool rds_avl)
{
    ALOGD("fm_rds_avail_status_cb: %d", rds_avl);
    if (!checkCallbackThread())
        return;

    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_rdsAvlStsCallback, (jboolean) rds_avl);
}

void fm_af_list_update_cb(uint16_t *af_list)
{
    ALOGD("AF_LIST");
    jbyteArray af_buffer = NULL;

    if (!checkCallbackThread())
        return;

    af_buffer = mCallbackEnv->NewByteArray(STD_BUF_SIZE);
    if (af_buffer == NULL) {
        ALOGE(" af list allocate failed :");
        return;
    }

    mCallbackEnv->SetByteArrayRegion(af_buffer, 0, STD_BUF_SIZE,(jbyte *)af_list);
    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_aflistCallback,af_buffer);
    mCallbackEnv->DeleteLocalRef(af_buffer);
}

void fm_rt_update_cb(char *rt)
{
    ALOGD("RT_EVT: " );
    jbyteArray rt_buff = NULL;
    int len;

    if (!checkCallbackThread())
        return;

    len  = (int)(rt[0] & 0xFF);
    ALOGD(" rt data len=%d :",len);
    len = len+5;

    ALOGD(" rt data len=%d :",len);
    rt_buff = mCallbackEnv->NewByteArray(len);
    if (rt_buff == NULL) {
        ALOGE(" ps data allocate failed :");
        return;
    }

    mCallbackEnv->SetByteArrayRegion(rt_buff, 0, len,(jbyte *)rt);
    jbyte* bytes= mCallbackEnv->GetByteArrayElements(rt_buff,0);

    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_rtCallback,rt_buff);
    mCallbackEnv->DeleteLocalRef(rt_buff);
}

void fm_ps_update_cb(char *ps)
{
    jbyteArray ps_data = NULL;
    int len;
    int numPs;
    if (!checkCallbackThread())
        return;

    numPs  = (int)(ps[0] & 0xFF);
    len = (numPs *8)+5;

    ALOGD(" ps data len=%d :",len);
    ps_data = mCallbackEnv->NewByteArray(len);
    if(ps_data == NULL) {
       ALOGE(" ps data allocate failed :");
       return;
    }

    mCallbackEnv->SetByteArrayRegion(ps_data, 0, len,(jbyte *)ps);
    jbyte* bytes= mCallbackEnv->GetByteArrayElements(ps_data,0);
    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_psInfoCallback,ps_data);
    mCallbackEnv->DeleteLocalRef(ps_data);
}

void fm_oda_update_cb(void)
{
    ALOGD("ODA_EVT");
}

void fm_rt_plus_update_cb(char *rt_plus)
{
    jbyteArray RtPlus = NULL;
    ALOGD("RT_PLUS");
    int len;

    len =  (int)(rt_plus[0] & 0xFF);
    ALOGD(" rt plus len=%d :",len);
    if (!checkCallbackThread())
        return;

    RtPlus = mCallbackEnv->NewByteArray(len);
    if (RtPlus == NULL) {
        ALOGE(" rt plus data allocate failed :");
        return;
    }
    mCallbackEnv->SetByteArrayRegion(RtPlus, 0, len,(jbyte *)rt_plus);
    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_rtplusCallback,RtPlus);
    mCallbackEnv->DeleteLocalRef(RtPlus);
}

void fm_ert_update_cb(char *ert)
{
    ALOGD("ERT_EVT");
    jbyteArray ert_buff = NULL;
    int len;

    if (!checkCallbackThread())
        return;

    len = (int)(ert[0] & 0xFF);
    len = len+3;

    ALOGI(" ert data len=%d :",len);
    ert_buff = mCallbackEnv->NewByteArray(len);
    if (ert_buff == NULL) {
        ALOGE(" ert data allocate failed :");
        return;
    }

    mCallbackEnv->SetByteArrayRegion(ert_buff, 0, len,(jbyte *)ert);
   // jbyte* bytes= mCallbackEnv->GetByteArrayElements(ert_buff,0);
    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_ertCallback,ert_buff);
    mCallbackEnv->DeleteLocalRef(ert_buff);
}

void fm_ext_country_code_cb(char *ecc)
{
    ALOGI("Extended Contry code ");
    jbyteArray ecc_buff = NULL;
    int len;

    if (!checkCallbackThread())
        return;

    len = (int)(ecc[0] & 0xFF);

    ALOGI(" ecc data len=%d :",len);
    ecc_buff = mCallbackEnv->NewByteArray(len);
    if (ecc_buff == NULL) {
        ALOGE(" ecc data allocate failed :");
        return;
    }
    mCallbackEnv->SetByteArrayRegion(ecc_buff, 0, len,(jbyte *)ecc);
    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_eccCallback,ecc_buff);
    mCallbackEnv->DeleteLocalRef(ecc_buff);
}


void rds_grp_cntrs_rsp_cb(char * evt_buffer)
{
   ALOGD("rds_grp_cntrs_rsp_cb");
}

void rds_grp_cntrs_ext_rsp_cb(char * evt_buffer)
{
   ALOGE("rds_grp_cntrs_ext_rsp_cb");
}

void fm_disabled_cb(void)
{
    ALOGE("DISABLE");
    if (!checkCallbackThread())
        return;

    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_disableCallback);
    mCallbacksObjCreated = false;
}

void fm_peek_rsp_cb(char *peek_rsp) {
    ALOGD("fm_peek_rsp_cb");
}

void fm_ssbi_peek_rsp_cb(char *ssbi_peek_rsp){
    ALOGD("fm_ssbi_peek_rsp_cb");
}

void fm_agc_gain_rsp_cb(char *agc_gain_rsp){
    ALOGE("fm_agc_gain_rsp_cb");
}

void fm_ch_det_th_rsp_cb(char *ch_det_rsp){
    ALOGD("fm_ch_det_th_rsp_cb");
}

static void fm_thread_evt_cb(unsigned int event) {
    JavaVM* vm = AndroidRuntime::getJavaVM();
    if (event  == 0) {
        JavaVMAttachArgs args;
        char name[] = "FM Service Callback Thread";
        args.version = JNI_VERSION_1_6;
        args.name = name;
        args.group = NULL;
       vm->AttachCurrentThread(&mCallbackEnv, &args);
        ALOGD("Callback thread attached: %p", mCallbackEnv);
    } else if (event == 1) {
        if (!checkCallbackThread()) {
            ALOGE("Callback: '%s' is not called on the correct thread", __FUNCTION__);
            return;
        }
        vm->DetachCurrentThread();
    }
}

static void fm_get_sig_thres_cb(int val, int status)
{
    ALOGD("Get signal Thres callback");

    if (!checkCallbackThread())
        return;

    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_getSigThCallback, val, status);
}

static void fm_get_ch_det_thr_cb(int val, int status)
{
    ALOGD("fm_get_ch_det_thr_cb");

    if (!checkCallbackThread())
        return;

    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_getChDetThrCallback, val, status);
}

static void fm_set_ch_det_thr_cb(int status)
{
    ALOGD("fm_set_ch_det_thr_cb");

    if (!checkCallbackThread())
        return;

    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_setChDetThrCallback, status);
}

static void fm_def_data_read_cb(int val, int status)
{
    ALOGD("fm_def_data_read_cb");

    if (!checkCallbackThread())
        return;

    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_defDataRdCallback, val, status);
}

static void fm_def_data_write_cb(int status)
{
    ALOGD("fm_def_data_write_cb");

    if (!checkCallbackThread())
        return;

    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_defDataWrtCallback, status);
}

static void fm_get_blend_cb(int val, int status)
{
    ALOGD("fm_get_blend_cb");

    if (!checkCallbackThread())
        return;

    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_getBlendCallback, val, status);
}

static void fm_set_blend_cb(int status)
{
    ALOGD("fm_set_blend_cb");

    if (!checkCallbackThread())
        return;

    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_setBlendCallback, status);
}

static void fm_get_station_param_cb(int val, int status)
{
    ALOGD("fm_get_station_param_cb");

    if (!checkCallbackThread())
        return;

    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_getStnParamCallback, val, status);
}

static void fm_get_station_debug_param_cb(int val, int status)
{
    ALOGD("fm_get_station_debug_param_cb");

    if (!checkCallbackThread())
        return;

    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_getStnDbgParamCallback, val, status);
}

static void fm_enable_slimbus_cb(int status)
{
    ALOGD("++fm_enable_slimbus_cb mCallbacksObjCreatedtor: %d", mCallbacksObjCreated);
    slimbus_flag = 1;
    if (mCallbacksObjCreated == false) {
        jobject javaObjectRef =  mCallbackEnv->NewObject(javaClassRef, method_FmReceiverJNICtor);
        mCallbacksObj = javaObjectRef;
        mCallbacksObjCreated = true;
        mCallbackEnv->CallVoidMethod(mCallbacksObj, method_enableSlimbusCallback, status);
        return;
    }

    if (!checkCallbackThread())
        return;

    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_enableSlimbusCallback, status);
    ALOGD("--fm_enable_slimbus_cb");
}

static void fm_enable_softmute_cb(int status)
{
    ALOGD("++fm_enable_softmute_cb");

    if (!checkCallbackThread())
        return;

    mCallbackEnv->CallVoidMethod(mCallbacksObj, method_enableSoftMuteCallback, status);
    ALOGD("--fm_enable_softmute_cb");
}

fm_interface_t *vendor_interface;
static   fm_hal_callbacks_t fm_callbacks = {
    sizeof(fm_callbacks),
    fm_enabled_cb,
    fm_tune_cb,
    fm_seek_cmpl_cb,
    fm_scan_next_cb,
    fm_srch_list_cb,
    fm_stereo_status_cb,
    fm_rds_avail_status_cb,
    fm_af_list_update_cb,
    fm_rt_update_cb,
    fm_ps_update_cb,
    fm_oda_update_cb,
    fm_rt_plus_update_cb,
    fm_ert_update_cb,
    fm_disabled_cb,
    rds_grp_cntrs_rsp_cb,
    rds_grp_cntrs_ext_rsp_cb,
    fm_peek_rsp_cb,
    fm_ssbi_peek_rsp_cb,
    fm_agc_gain_rsp_cb,
    fm_ch_det_th_rsp_cb,
    fm_ext_country_code_cb,
    fm_thread_evt_cb,
    fm_get_sig_thres_cb,
    fm_get_ch_det_thr_cb,
    fm_def_data_read_cb,
    fm_get_blend_cb,
    fm_set_ch_det_thr_cb,
    fm_def_data_write_cb,
    fm_set_blend_cb,
    fm_get_station_param_cb,
    fm_get_station_debug_param_cb,
    fm_enable_slimbus_cb,
    fm_enable_softmute_cb
};
/* native interface */

static void get_property(int ptype, char *value)
{
    std::vector<vendor_property_t> vPropList;
    bt_configstore_intf->get_vendor_properties(ptype, vPropList);

    for (auto&& vendorProp : vPropList) {
        if (vendorProp.type == ptype) {
            strlcpy(value, vendorProp.value,PROPERTY_VALUE_MAX);
        }
    }
}

/********************************************************************
 * Current JNI
 *******************************************************************/

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_getFreqNative
    (JNIEnv * env, jobject thiz, jint fd)
{
    int err;
    long freq;

    err = vendor_interface->get_fm_ctrl(V4L2_CID_PRV_IRIS_FREQ, (int *)&freq);
    if (err == FM_JNI_SUCCESS) {
        err = freq;
    } else {
        err = FM_JNI_FAILURE;
        ALOGE("%s: get freq failed\n", LOG_TAG);
    }
    return err;
}

/*native interface */
static jint android_hardware_fmradio_FmReceiverJNI_setFreqNative
    (JNIEnv * env, jobject thiz, jint fd, jint freq)
{
    int err;

    err = vendor_interface->set_fm_ctrl(V4L2_CID_PRV_IRIS_FREQ, freq);

    return err;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_setControlNative
    (JNIEnv * env, jobject thiz, jint fd, jint id, jint value)
{
    int err;
    ALOGE("id(%x) value: %x\n", id, value);

    err = vendor_interface->set_fm_ctrl(id, value);

    return err;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_getControlNative
    (JNIEnv * env, jobject thiz, jint fd, jint id)
{
    int err;
    long val;

    ALOGE("id(%x)\n", id);
    err = vendor_interface->get_fm_ctrl(id, (int *)&val);
    if (err < 0) {
        ALOGE("%s: get control failed, id: %d\n", LOG_TAG, id);
        err = FM_JNI_FAILURE;
    } else {
        err = val;
    }
    return err;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_startSearchNative
    (JNIEnv * env, jobject thiz, jint fd, jint dir)
{
    int err;

    err = vendor_interface->set_fm_ctrl(V4L2_CID_PRV_IRIS_SEEK, dir);
    if (err < 0) {
        ALOGE("%s: search failed, dir: %d\n", LOG_TAG, dir);
        err = FM_JNI_FAILURE;
    } else {
        err = FM_JNI_SUCCESS;
    }

    return err;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_cancelSearchNative
    (JNIEnv * env, jobject thiz, jint fd)
{
    int err;

    err = vendor_interface->set_fm_ctrl(V4L2_CID_PRV_SRCHON, 0);
    if (err < 0) {
        ALOGE("%s: cancel search failed\n", LOG_TAG);
        err = FM_JNI_FAILURE;
    } else {
        err = FM_JNI_SUCCESS;
    }

    return err;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_getRSSINative
    (JNIEnv * env, jobject thiz, jint fd)
{
    int err;
    long rmssi;

    err = vendor_interface->get_fm_ctrl(V4L2_CID_PRV_IRIS_RMSSI, (int *)&rmssi);
    if (err < 0) {
        ALOGE("%s: Get Rssi failed", LOG_TAG);
        err = FM_JNI_FAILURE;
    } else {
        err = FM_JNI_SUCCESS;
    }

    return err;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_setBandNative
    (JNIEnv * env, jobject thiz, jint fd, jint low, jint high)
{
    int err;

    err = vendor_interface->set_fm_ctrl(V4L2_CID_PRV_IRIS_UPPER_BAND, high);
    if (err < 0) {
        ALOGE("%s: set band failed, high: %d\n", LOG_TAG, high);
        err = FM_JNI_FAILURE;
        return err;
    }
    err = vendor_interface->set_fm_ctrl(V4L2_CID_PRV_IRIS_LOWER_BAND, low);
    if (err < 0) {
        ALOGE("%s: set band failed, low: %d\n", LOG_TAG, low);
        err = FM_JNI_FAILURE;
    } else {
        err = FM_JNI_SUCCESS;
    }

    return err;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_getLowerBandNative
    (JNIEnv * env, jobject thiz, jint fd)
{
    int err;
    ULINT freq;

    err = vendor_interface->get_fm_ctrl(V4L2_CID_PRV_IRIS_LOWER_BAND, (int *)&freq);
    if (err < 0) {
        ALOGE("%s: get lower band failed\n", LOG_TAG);
        err = FM_JNI_FAILURE;
    } else {
        err = freq;
    }

    return err;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_getUpperBandNative
    (JNIEnv * env, jobject thiz, jint fd)
{
    int err;
    ULINT freq;

    err = vendor_interface->get_fm_ctrl(V4L2_CID_PRV_IRIS_UPPER_BAND, (int *)&freq);
    if (err < 0) {
        ALOGE("%s: get upper band failed\n", LOG_TAG);
        err = FM_JNI_FAILURE;
    } else {
        err = freq;
    }

    return err;
}

static jint android_hardware_fmradio_FmReceiverJNI_setMonoStereoNative
    (JNIEnv * env, jobject thiz, jint fd, jint val)
{

    int err;

    err = vendor_interface->set_fm_ctrl(V4L2_CID_PRV_IRIS_AUDIO_MODE, val);
    if (err < 0) {
        ALOGE("%s: set audio mode failed\n", LOG_TAG);
        err = FM_JNI_FAILURE;
    } else {
        err = FM_JNI_SUCCESS;
    }

    return err;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_getRawRdsNative
 (JNIEnv * env, jobject thiz, jint fd, jbooleanArray buff, jint count)
{

    return (read (fd, buff, count));

}

static jint android_hardware_fmradio_FmReceiverJNI_configureSpurTable
    (JNIEnv * env, jobject thiz, jint fd)
{
    ALOGD("->android_hardware_fmradio_FmReceiverJNI_configureSpurTable\n");

    return FM_JNI_SUCCESS;
}

static jint android_hardware_fmradio_FmReceiverJNI_setPSRepeatCountNative
    (JNIEnv * env, jobject thiz, jint fd, jint repCount)
{

    ALOGE("->android_hardware_fmradio_FmReceiverJNI_setPSRepeatCountNative\n");

    return FM_JNI_SUCCESS;

}

static jint android_hardware_fmradio_FmReceiverJNI_setTxPowerLevelNative
    (JNIEnv * env, jobject thiz, jint fd, jint powLevel)
{

    ALOGE("->android_hardware_fmradio_FmReceiverJNI_setTxPowerLevelNative\n");

    return FM_JNI_SUCCESS;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_setSpurDataNative
 (JNIEnv * env, jobject thiz, jint fd, jshortArray buff, jint count)
{
    ALOGE("entered JNI's setSpurDataNative\n");

    return FM_JNI_SUCCESS;
}

static jint android_hardware_fmradio_FmReceiverJNI_enableSlimbusNative
 (JNIEnv * env, jobject thiz, jint fd, jint val)
{
    ALOGD("%s: val = %d\n", __func__, val);
    int err = JNI_ERR;
    err = vendor_interface->set_fm_ctrl(V4L2_CID_PRV_ENABLE_SLIMBUS, val);

    return err;
}

static jboolean android_hardware_fmradio_FmReceiverJNI_getFmStatsPropNative
 (JNIEnv* env)
{
    jboolean ret;
    char value[PROPERTY_VALUE_MAX] = {'\0'};
    get_property(FM_STATS_PROP, value);
    if (!strncasecmp(value, "true", sizeof("true"))) {
        ret = true;
    } else {
        ret = false;
    }

    return ret;
}

static jint android_hardware_fmradio_FmReceiverJNI_getFmCoexPropNative
(JNIEnv * env, jobject thiz, jint fd, jint prop)
{
    jint ret;
    int property = (int)prop;
    char value[PROPERTY_VALUE_MAX] = {'\0'};

    if (property == FMWAN_RATCONF) {
        get_property(FM_PROP_WAN_RATCONF, value);
    } else if (property == FMBTWLAN_LPFENABLER) {
        get_property(FM_PROP_BTWLAN_LPFENABLER, value);
    } else {
        ALOGE("%s: invalid get property prop = %d\n", __func__, property);
    }

    ret = atoi(value);
    ALOGI("%d:: ret  = %d",property, ret);
    return ret;
}

static jint android_hardware_fmradio_FmReceiverJNI_enableSoftMuteNative
 (JNIEnv * env, jobject thiz, jint fd, jint val)
{
    ALOGD("%s: val = %d\n", __func__, val);
    int err = JNI_ERR;
    err = vendor_interface->set_fm_ctrl(V4L2_CID_PRV_SOFT_MUTE, val);

    return err;
}

static jstring android_hardware_fmradio_FmReceiverJNI_getSocNameNative
 (JNIEnv* env)
{
    ALOGI("%s, bt_configstore_intf: %p isSocNameAvailable: %d",
        __FUNCTION__, bt_configstore_intf, isSocNameAvailable);

    if (bt_configstore_intf != NULL && isSocNameAvailable == false) {
       std::vector<vendor_property_t> vPropList;

       bt_configstore_intf->get_vendor_properties(BT_PROP_SOC_TYPE, vPropList);
       for (auto&& vendorProp : vPropList) {
          if (vendorProp.type == BT_PROP_SOC_TYPE) {
            strlcpy(soc_name, vendorProp.value, sizeof(soc_name));
            isSocNameAvailable = true;
            ALOGI("%s:: soc_name = %s",__func__, soc_name);
          }
       }
    }
    return env->NewStringUTF(soc_name);
}

static void classInitNative(JNIEnv* env, jclass clazz) {

    ALOGI("ClassInit native called \n");
    jclass dataClass = env->FindClass("qcom/fmradio/FmReceiverJNI");
    javaClassRef = (jclass) env->NewGlobalRef(dataClass);
    lib_handle = dlopen(FM_LIBRARY_NAME, RTLD_NOW);
    if (!lib_handle) {
        ALOGE("%s unable to open %s: %s", __func__, FM_LIBRARY_NAME, dlerror());
        goto error;
    }
    ALOGI("Opened %s shared object library successfully", FM_LIBRARY_NAME);

    ALOGI("Obtaining handle: '%s' to the shared object library...", FM_LIBRARY_SYMBOL_NAME);
    vendor_interface = (fm_interface_t *)dlsym(lib_handle, FM_LIBRARY_SYMBOL_NAME);
    if (!vendor_interface) {
        ALOGE("%s unable to find symbol %s in %s: %s", __func__, FM_LIBRARY_SYMBOL_NAME, FM_LIBRARY_NAME, dlerror());
        goto error;
    }

    method_psInfoCallback = env->GetMethodID(javaClassRef, "PsInfoCallback", "([B)V");
    method_rtCallback = env->GetMethodID(javaClassRef, "RtCallback", "([B)V");
    method_ertCallback = env->GetMethodID(javaClassRef, "ErtCallback", "([B)V");
    method_eccCallback = env->GetMethodID(javaClassRef, "EccCallback", "([B)V");
    method_rtplusCallback = env->GetMethodID(javaClassRef, "RtPlusCallback", "([B)V");
    method_aflistCallback = env->GetMethodID(javaClassRef, "AflistCallback", "([B)V");
    method_enableCallback = env->GetMethodID(javaClassRef, "enableCallback", "()V");
    method_tuneCallback = env->GetMethodID(javaClassRef, "tuneCallback", "(I)V");
    method_seekCmplCallback = env->GetMethodID(javaClassRef, "seekCmplCallback", "(I)V");
    method_scanNxtCallback = env->GetMethodID(javaClassRef, "scanNxtCallback", "()V");
    method_srchListCallback = env->GetMethodID(javaClassRef, "srchListCallback", "([B)V");
    method_stereostsCallback = env->GetMethodID(javaClassRef, "stereostsCallback", "(Z)V");
    method_rdsAvlStsCallback = env->GetMethodID(javaClassRef, "rdsAvlStsCallback", "(Z)V");
    method_disableCallback = env->GetMethodID(javaClassRef, "disableCallback", "()V");
    method_getSigThCallback = env->GetMethodID(javaClassRef, "getSigThCallback", "(II)V");
    method_getChDetThrCallback = env->GetMethodID(javaClassRef, "getChDetThCallback", "(II)V");
    method_defDataRdCallback = env->GetMethodID(javaClassRef, "DefDataRdCallback", "(II)V");
    method_getBlendCallback = env->GetMethodID(javaClassRef, "getBlendCallback", "(II)V");
    method_setChDetThrCallback = env->GetMethodID(javaClassRef, "setChDetThCallback","(I)V");
    method_defDataWrtCallback = env->GetMethodID(javaClassRef, "DefDataWrtCallback", "(I)V");
    method_setBlendCallback = env->GetMethodID(javaClassRef, "setBlendCallback", "(I)V");
    method_getStnParamCallback = env->GetMethodID(javaClassRef, "getStnParamCallback", "(II)V");
    method_getStnDbgParamCallback = env->GetMethodID(javaClassRef, "getStnDbgParamCallback", "(II)V");
    method_enableSlimbusCallback = env->GetMethodID(javaClassRef, "enableSlimbusCallback", "(I)V");
    method_enableSoftMuteCallback = env->GetMethodID(javaClassRef, "enableSoftMuteCallback", "(I)V");
    method_FmReceiverJNICtor = env->GetMethodID(javaClassRef,"<init>","()V");

    return;
error:
    vendor_interface = NULL;
    if (lib_handle)
        dlclose(lib_handle);
    lib_handle = NULL;
}

static void initNative(JNIEnv *env, jobject object) {
    int status;
    ALOGI("Init native called \n");

    if (vendor_interface) {
        ALOGI("Initializing the FM HAL module & registering the JNI callback functions...");
        status = vendor_interface->hal_init(&fm_callbacks);
        if (status) {
            ALOGE("%s unable to initialize vendor library: %d", __func__, status);
            return;
        }
        ALOGI("***** FM HAL Initialization complete *****\n");
    }
    mCallbacksObj = env->NewGlobalRef(object);
}

static void cleanupNative(JNIEnv *env, jobject object) {

    if (mCallbacksObj != NULL) {
        env->DeleteGlobalRef(mCallbacksObj);
        mCallbacksObj = NULL;
    }
}
/*
 * JNI registration.
 */
static JNINativeMethod gMethods[] = {
        /* name, signature, funcPtr */
        { "classInitNative", "()V", (void*)classInitNative},
        { "initNative", "()V", (void*)initNative},
        {"cleanupNative", "()V", (void *) cleanupNative},
        { "getFreqNative", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getFreqNative},
        { "setFreqNative", "(II)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setFreqNative},
        { "getControlNative", "(II)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getControlNative},
        { "setControlNative", "(III)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setControlNative},
        { "startSearchNative", "(II)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_startSearchNative},
        { "cancelSearchNative", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_cancelSearchNative},
        { "getRSSINative", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getRSSINative},
        { "setBandNative", "(III)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setBandNative},
        { "getLowerBandNative", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getLowerBandNative},
        { "getUpperBandNative", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getUpperBandNative},
        { "setMonoStereoNative", "(II)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setMonoStereoNative},
        { "getRawRdsNative", "(I[BI)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getRawRdsNative},
        { "setPSRepeatCountNative", "(II)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setPSRepeatCountNative},
        { "setTxPowerLevelNative", "(II)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setTxPowerLevelNative},
        { "configureSpurTable", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_configureSpurTable},
        { "setSpurDataNative", "(I[SI)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setSpurDataNative},
        { "enableSlimbus", "(II)I",
             (void*)android_hardware_fmradio_FmReceiverJNI_enableSlimbusNative},
        { "enableSoftMute", "(II)I",
             (void*)android_hardware_fmradio_FmReceiverJNI_enableSoftMuteNative},
        {"getSocNameNative", "()Ljava/lang/String;",
             (void*) android_hardware_fmradio_FmReceiverJNI_getSocNameNative},
        {"getFmStatsPropNative", "()Z",
             (void*) android_hardware_fmradio_FmReceiverJNI_getFmStatsPropNative},
        { "getFmCoexPropNative", "(II)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getFmCoexPropNative},
};

int register_android_hardware_fm_fmradio(JNIEnv* env)
{
        ALOGI("%s, bt_configstore_intf", __FUNCTION__, bt_configstore_intf);
        if (bt_configstore_intf == NULL) {
          load_bt_configstore_lib();
        }

        return jniRegisterNativeMethods(env, "qcom/fmradio/FmReceiverJNI", gMethods, NELEM(gMethods));
}

int deregister_android_hardware_fm_fmradio(JNIEnv* env)
{
        if (bt_configstore_lib_handle) {
            dlclose(bt_configstore_lib_handle);
            bt_configstore_lib_handle = NULL;
            bt_configstore_intf = NULL;
        }
        return 0;
}

int load_bt_configstore_lib() {
    const char* sym = BT_CONFIG_STORE_INTERFACE_STRING;

    bt_configstore_lib_handle = dlopen("libbtconfigstore.so", RTLD_NOW);
    if (!bt_configstore_lib_handle) {
        const char* err_str = dlerror();
        ALOGE("%s:: failed to load Bt Config store library, error= %s",
            __func__, (err_str) ? err_str : "error unknown");
        goto error;
    }

    // Get the address of the bt_configstore_interface_t.
    bt_configstore_intf = (bt_configstore_interface_t*)dlsym(bt_configstore_lib_handle, sym);
    if (!bt_configstore_intf) {
        ALOGE("%s:: failed to load symbol from bt config store library = %s",
            __func__, sym);
        goto error;
    }

    // Success.
    ALOGI("%s::  loaded HAL: bt_configstore_interface_t = %p , bt_configstore_lib_handle= %p",
        __func__, bt_configstore_intf, bt_configstore_lib_handle);
    return 0;

  error:
    if (bt_configstore_lib_handle) {
      dlclose(bt_configstore_lib_handle);
      bt_configstore_lib_handle = NULL;
      bt_configstore_intf = NULL;
    }

    return -EINVAL;
}

} // end namespace

jint JNI_OnLoad(JavaVM *jvm, void *reserved)
{
    JNIEnv *e;
    int status;
    g_jVM = jvm;

    ALOGI("FM : Loading QCOMM FM-JNI");
    if (jvm->GetEnv((void **)&e, JNI_VERSION_1_6)) {
        ALOGE("JNI version mismatch error");
        return JNI_ERR;
    }

    if ((status = android::register_android_hardware_fm_fmradio(e)) < 0) {
        ALOGE("jni adapter service registration failure, status: %d", status);
        return JNI_ERR;
    }
    return JNI_VERSION_1_6;
}

jint JNI_OnUnLoad(JavaVM *jvm, void *reserved)
{
    JNIEnv *e;
    int status;
    g_jVM = jvm;

    ALOGI("FM : unLoading QCOMM FM-JNI");
    if (jvm->GetEnv((void **)&e, JNI_VERSION_1_6)) {
        ALOGE("JNI version mismatch error");
        return JNI_ERR;
    }

    if ((status = android::deregister_android_hardware_fm_fmradio(e)) < 0) {
        ALOGE("jni adapter service unregistration failure, status: %d", status);
        return JNI_ERR;
    }
    return JNI_VERSION_1_6;
}
