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

#include "fmr.h"

#ifdef LOG_TAG
#  undef LOG_TAG
#endif
#define LOG_TAG "FMLIB_CORE"

namespace mozilla {
namespace hal_impl {

char *FM_LIBRARY_NAME = "fm_helium.so";
char *FM_LIBRARY_SYMBOL_NAME = "FM_HELIUM_LIB_INTERFACE";
void *lib_handle;

fm_interface_t *vendor_interface;
fm_hal_callbacks_t fm_callbacks = {
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
void get_property(int ptype, char *value)
{
    ALOGE("%s: get_property\n", LOG_TAG);
}

/********************************************************************
 * Switch JNI api to normal API
 *******************************************************************/

/* native interface */
int getFreqNative(int fd)
{
    ALOGD("->getFreqNative\n");
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
int setFreqNative(int fd, int freq)
{
    ALOGD("->setFreqNative, freq: %x\n", freq);
    int err;

    err = vendor_interface->set_fm_ctrl(V4L2_CID_PRV_IRIS_FREQ, freq);

    return err;
}

/* native interface */
int setControlNative(int fd, int id, int value)
{
    ALOGD("->setControlNative\n");
    int err;
    ALOGD("id(%x) value: %x\n", id, value);

    err = vendor_interface->set_fm_ctrl(id, value);

    return err;
}

/* native interface */
int getControlNative(int fd, int id)
{
    ALOGD("->getControlNative\n");
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
int startSearchNative(int fd, int dir)
{
    ALOGD("->startSearchNative\n");
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
int cancelSearchNative(int fd)
{
    ALOGD("->cancelSearchNative\n");
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
int getRSSINative(int fd)
{
    ALOGD("->getRSSINative\n");
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
int setBandNative(int fd, int low, int high)
{
    ALOGD("->setBandNative\n");
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
int getLowerBandNative(int fd)
{
    ALOGD("->getLowerBandNative\n");
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
int getUpperBandNative(int fd)
{
    ALOGD("->getUpperBandNative\n");
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

int setMonoStereoNative(int fd, int val)
{
    ALOGD("->setMonoStereoNative\n");
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
int getRawRdsNative(int fd, std::vector<bool>& buff, int count)
{
    ALOGD("->getRawRdsNative\n");
    return FM_JNI_SUCCESS;
}

int configureSpurTable(int fd)
{
    ALOGD("->configureSpurTable\n");

    return FM_JNI_SUCCESS;
}

int setPSRepeatCountNative(int fd, int repCount)
{
    ALOGE("->setPSRepeatCountNative\n");

    return FM_JNI_SUCCESS;
}

int setTxPowerLevelNative(int fd, int powLevel)
{
    ALOGE("->setTxPowerLevelNative\n");

    return FM_JNI_SUCCESS;
}

/* native interface */
int setSpurDataNative(int fd, std::vector<short>& buff, int count)
{
    ALOGD("->setSpurDataNative\n");

    return FM_JNI_SUCCESS;
}

int enableSlimbusNative(int fd, int val)
{
    ALOGD("->enableSlimbusNative\n");
    ALOGD("%s: val = %d\n", __func__, val);
    int err = FM_JNI_FAILURE;
    err = vendor_interface->set_fm_ctrl(V4L2_CID_PRV_ENABLE_SLIMBUS, val);

    return err;
}

bool getFmStatsPropNative()
{
    ALOGD("->getFmStatsPropNative\n");
    bool ret;
    char value[PROPERTY_VALUE_MAX] = {'\0'};
    get_property(FM_STATS_PROP, value);
    if (!strncasecmp(value, "true", sizeof("true"))) {
        ret = true;
    } else {
        ret = false;
    }

    return ret;
}

int getFmCoexPropNative(int fd, int prop)
{
    ALOGD("->getFmCoexPropNative\n");
    int ret;
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

int enableSoftMuteNative(int fd, int val)
{
    ALOGD("->enableSoftMuteNative\n");
    ALOGD("%s: val = %d\n", __func__, val);
    int err = FM_JNI_FAILURE;
    err = vendor_interface->set_fm_ctrl(V4L2_CID_PRV_SOFT_MUTE, val);

    return err;
}

void classInitNative() {

    ALOGI("ClassInit native called \n");
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

    return;
error:
    vendor_interface = NULL;
    if (lib_handle)
        dlclose(lib_handle);
    lib_handle = NULL;
}

void initNative() {
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
}

}  // namespace hal_impl
}  // namespace mozilla
