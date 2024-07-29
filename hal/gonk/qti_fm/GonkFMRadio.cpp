/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fmr.h"

#include "Hal.h"
#include "HalLog.h"
#include "nsThreadUtils.h"
#include "mozilla/FileUtils.h"

#include <cutils/properties.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/types.h>

#define DISABLE_SLIMBUS_DATA_PORT 0
#define ENABLE_SLIMBUS_DATA_PORT 1
#define ENABLE_SOFT_MUTE 1
#define FM_RDS_GRP_3A 64


namespace mozilla {
namespace hal_impl {

void SetFMRadioFrequency(const uint32_t frequency);
uint32_t GetFMRadioFrequency();

static int sRadioFD = -1;
static bool sRadioEnabled;
static bool sRDSEnabled;
static pthread_t sRadioThread;
static hal::FMRadioSettings sRadioSettings;
static int sFrequency;
static bool sRDSSupported;

int slimbus_flag = 0;

/* native callbacks */
void fm_enabled_cb(void) {
  ALOGD("Entered %s, slimbus_flag: %d", __func__, slimbus_flag);

  sRadioEnabled = true;
  ALOGD("exit  %s", __func__);
}

void fm_tune_cb(int Freq)
{
  ALOGD("TUNE:Freq:%d", Freq);
  if (Freq != sFrequency) {
    SetFMRadioFrequency(Freq);
  }
}

void fm_seek_cmpl_cb(int Freq)
{
  ALOGI("SEEK_CMPL: Freq: %d", Freq);
}

void fm_scan_next_cb(void)
{
  ALOGI("SCAN_NEXT");
}

void fm_srch_list_cb(uint16_t *scan_tbl)
{
  ALOGI("SRCH_LIST");
}

void fm_stereo_status_cb(bool stereo)
{
  ALOGI("STEREO: %d", stereo);
}

void fm_rds_avail_status_cb(bool rds_avl)
{
  ALOGD("fm_rds_avail_status_cb: %d", rds_avl);
  sRDSSupported = rds_avl;
}

void fm_af_list_update_cb(uint16_t *af_list)
{
  ALOGD("AF_LIST");
}

void fm_rt_update_cb(char *rt)
{
  ALOGD("fm_rt_update_cb: " );
}

void fm_ps_update_cb(char *ps)
{
  ALOGD("fm_ps_update_cb: " );
}

void fm_oda_update_cb(void)
{
  ALOGD("ODA_EVT");
}

void fm_rt_plus_update_cb(char *rt_plus)
{
  ALOGD("fm_rt_plus_update_cb");
}

void fm_ert_update_cb(char *ert)
{
  ALOGD("ERT_EVT");
}

void fm_ext_country_code_cb(char *ecc)
{
  ALOGI("Extended Contry code ");
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
  ALOGE("fm_disabled_cb");
  sRadioEnabled = false;
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

void fm_thread_evt_cb(unsigned int event) {
  ALOGD("fm_peek_rsp_cb");
}

void fm_get_sig_thres_cb(int val, int status)
{
  ALOGD("Get signal Thres callback");
}

void fm_get_ch_det_thr_cb(int val, int status)
{
  ALOGD("fm_get_ch_det_thr_cb");
}

void fm_set_ch_det_thr_cb(int status)
{
  ALOGD("fm_set_ch_det_thr_cb");
}

void fm_def_data_read_cb(int val, int status)
{
  ALOGD("fm_def_data_read_cb");
}

void fm_def_data_write_cb(int status)
{
  ALOGD("fm_def_data_write_cb");
}

void fm_get_blend_cb(int val, int status)
{
  ALOGD("fm_get_blend_cb");
}

void fm_set_blend_cb(int status)
{
  ALOGD("fm_set_blend_cb");
}

void fm_get_station_param_cb(int val, int status)
{
  ALOGD("fm_get_station_param_cb");
}

void fm_get_station_debug_param_cb(int val, int status)
{
  ALOGD("fm_get_station_debug_param_cb");
}

void fm_enable_slimbus_cb(int status)
{
  ALOGD("++fm_enable_slimbus_cb status: %d", status);
  slimbus_flag = 1;
  ALOGD("--fm_enable_slimbus_cb");
}

void fm_enable_softmute_cb(int status)
{
  ALOGD("++fm_enable_softmute_cb, status: %d", status);
  ALOGD("--fm_enable_softmute_cb");
}

class RadioUpdate : public Runnable {
  hal::FMRadioOperation mOp;
  hal::FMRadioOperationStatus mStatus;

 public:
  RadioUpdate(hal::FMRadioOperation op, hal::FMRadioOperationStatus status)
      : Runnable("hal::RadioUpdate"), mOp(op), mStatus(status) {}

  NS_IMETHOD Run() override {
    hal::FMRadioOperationInformation info;
    info.operation() = mOp;
    info.status() = mStatus;
    info.frequency() = GetFMRadioFrequency();
    hal::NotifyFMRadioStatus(info);
    return NS_OK;
  }
};

/* Runs on the radio thread */
static void initMsmFMRadio(hal::FMRadioSettings& aInfo) {
  HAL_LOG("%d,initMsmFMRadio, sRadioEnabled %d", __LINE__, sRadioEnabled);
  mozilla::ScopedClose fd(sRadioFD);

  // Send command to enable/disable FM core
  enableSlimbusNative(fd, ENABLE_SLIMBUS_DATA_PORT);

  int rc = setControlNative(fd, V4L2_CID_PRIVATE_TAVARUA_STATE, FM_RECV);
  if (rc < 0) {
    HAL_LOG("Unable to turn on radio |%s|", strerror(errno));
    return;
  }

  rc = enableSoftMuteNative(fd, ENABLE_SOFT_MUTE);
  if (rc < 0) {
    HAL_LOG("enableSoftMute failed");
    return;
  }

  char propval[PROPERTY_VALUE_MAX];
  property_get("ro.fm.analogpath.supported", propval, "");
  bool supportAnalog = !strcmp(propval, "true");

  rc = setControlNative(fd, V4L2_CID_PRIVATE_TAVARUA_SET_AUDIO_PATH,
                        supportAnalog ? FM_ANALOG_PATH : FM_DIGITAL_PATH);
  if (rc < 0) {
    HAL_LOG("Unable to set audio path");
    return;
  }

  int preEmphasis = aInfo.preEmphasis() <= 50;
  rc = setControlNative(fd, V4L2_CID_PRIVATE_TAVARUA_EMPHASIS, preEmphasis);
  if (rc) {
    HAL_LOG("Unable to configure preemphasis");
    return;
  }

  int rdsStd = aInfo.rdsStd();
  rc = setControlNative(fd, V4L2_CID_PRIVATE_TAVARUA_RDS_STD, rdsStd);
  if (rc) {
    HAL_LOG("Unable to configure RDS");
    return;
  }

  int spacing;
  switch (aInfo.spaceType()) {
    case 50:
      spacing = FM_CH_SPACE_50KHZ;
      break;
    case 100:
      spacing = FM_CH_SPACE_100KHZ;
      break;
    case 200:
      spacing = FM_CH_SPACE_200KHZ;
      break;
    default:
      HAL_LOG("Unsupported space value - %d", aInfo.spaceType());
      return;
  }
  rc = setControlNative(fd, V4L2_CID_PRIVATE_TAVARUA_SPACING, spacing);
  if (rc) {
    HAL_LOG("Unable to configure spacing");
    return;
  }

  int lowerLimit = aInfo.lowerLimit();
  int upperLimit = aInfo.upperLimit();
  rc = setBandNative(fd, lowerLimit, upperLimit);
  if (rc < 0) {
    HAL_LOG("Unable to configure band limit");
    return;
  }

  /* setControlNative for V4L2_CID_PRIVATE_TAVARUA_REGION triggers the config change*/
  int bandRegion = aInfo.band();
  rc = setControlNative(fd, V4L2_CID_PRIVATE_TAVARUA_REGION, bandRegion);
  if (rc < 0) {
    HAL_LOG("Unable to configure band region");
    return;
  }

  //setRDSGrpMask(FM_RDS_GRP_3A);
  rc = setControlNative(fd, V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_MASK, FM_RDS_GRP_3A);
  if (rc < 0) {
    HAL_LOG("Unable to set RDS Group mask");
  }
  fd.forget();
  sRadioEnabled = true;
}

/* Runs on the radio thread */
static void* runMsmFMRadio(void*) {
  HAL_LOG("%d,runMsmFMRadio,sRadioEnabled %d", __LINE__, sRadioEnabled);
  classInitNative();
  initNative();
  initMsmFMRadio(sRadioSettings);

  if (!sRadioEnabled) {
    NS_DispatchToMainThread(new RadioUpdate(
        hal::FM_RADIO_OPERATION_ENABLE, hal::FM_RADIO_OPERATION_STATUS_FAIL));
    return nullptr;
  }

  HAL_LOG(
      "when open following line, press play/pause key, FM will change to next "
      "clearness channel");
  NS_DispatchToMainThread(new RadioUpdate(
      hal::FM_RADIO_OPERATION_ENABLE, hal::FM_RADIO_OPERATION_STATUS_SUCCESS));

  HAL_LOG("runMsmFMRadio finish\n");
  return nullptr;
}

/* This runs on the main thread but most of the
 * initialization is pushed to the radio thread. */
void EnableFMRadio(const hal::FMRadioSettings& aInfo) {
  if (sRadioEnabled) {
    HAL_LOG("%d,EnableFMRadio,Radio already enabled!", __LINE__);
    return;
  }

  HAL_LOG("At %d In (EnableFMRadio),sRadioEnabled %d", __LINE__, sRadioEnabled);

  hal::FMRadioOperationInformation info;
  info.operation() = hal::FM_RADIO_OPERATION_ENABLE;
  info.status() = hal::FM_RADIO_OPERATION_STATUS_FAIL;

  sRadioSettings = aInfo;
  {
    if (pthread_create(&sRadioThread, nullptr, runMsmFMRadio, nullptr)) {
      HAL_LOG("Couldn't create radio thread");
      hal::NotifyFMRadioStatus(info);
      return;
    }
  }
  HAL_LOG("At %d In (EnableFMRadio),exit", __LINE__);
}

void DisableFMRadio() {
  HAL_LOG("At %d In (DisableFMRadio),sRadioEnabled %d", __LINE__, sRadioEnabled);
  if (!sRadioEnabled) return;

  if (sRDSEnabled) hal::DisableRDS();

  if (sRadioEnabled) {
    int rc = setControlNative(sRadioFD, V4L2_CID_PRIVATE_TAVARUA_STATE, FM_OFF);
    if (rc < 0) {
      HAL_LOG("Unable to turn off radio");
    }

    pthread_join(sRadioThread, nullptr);
  }

  sRadioEnabled = false;
  close(sRadioFD);

  hal::FMRadioOperationInformation info;
  info.operation() = hal::FM_RADIO_OPERATION_DISABLE;
  info.status() = hal::FM_RADIO_OPERATION_STATUS_SUCCESS;
  hal::NotifyFMRadioStatus(info);
}

void FMRadioSeek(const hal::FMRadioSeekDirection& aDirection) {
  HAL_LOG("At %d In (FMRadioSeek), sRadioEnabled %d", __LINE__, sRadioEnabled);
  /*configure various search parameters and start search*/
  int mode = FM_RX_SRCH_MODE_SEEK; //default val for seek
  int rc = setControlNative(sRadioFD, V4L2_CID_PRIVATE_TAVARUA_SRCHMODE, mode);
  if (rc < 0) {
    HAL_LOG("setting of search mode failed");
    return;
  }

  int dwell = FM_RX_DWELL_PERIOD_1S; //default val for seek
  rc = setControlNative(sRadioFD, V4L2_CID_PRIVATE_TAVARUA_SCANDWELL, dwell);
  if (rc < 0) {
      HAL_LOG("setting of scan dwell time failed");
      return;
  }

  int dir = aDirection == hal::FMRadioSeekDirection::FM_RADIO_SEEK_DIRECTION_UP;
  rc = startSearchNative(sRadioFD, dir);
  NS_DispatchToMainThread(
      new RadioUpdate(hal::FM_RADIO_OPERATION_SEEK,
                      rc < 0 ? hal::FM_RADIO_OPERATION_STATUS_FAIL
                             : hal::FM_RADIO_OPERATION_STATUS_SUCCESS));

  if (rc < 0) {
    HAL_LOG("Could not initiate hardware seek");
    return;
  }

  NS_DispatchToMainThread(new RadioUpdate(
    hal::FM_RADIO_OPERATION_TUNE, hal::FM_RADIO_OPERATION_STATUS_SUCCESS));
}

void GetFMRadioSettings(hal::FMRadioSettings* aInfo) {
  HAL_LOG("At %d In (GetFMRadioSettings), sRadioEnabled %d", __LINE__, sRadioEnabled);
  if (!sRadioEnabled) {
    return;
  }

  int rc = getLowerBandNative(sRadioFD);
  if (rc < 0) {
    HAL_LOG("Could not query fm lower limit for settings");
    return;
  }
  aInfo->lowerLimit() = rc;
  rc = getUpperBandNative(sRadioFD);
  if (rc < 0) {
    HAL_LOG("Could not query fm upper limit for settings");
    return;
  }
  aInfo->upperLimit() = rc;
}

void SetFMRadioFrequency(const uint32_t frequency) {
  HAL_LOG("At %d In (SetFMRadioFrequency), sRadioEnabled %d", __LINE__, sRadioEnabled);
  if (!sRadioEnabled) {
    return;
  }
  HAL_LOG("SetFMRadioFrequency... frequency: %ld", frequency);
  int rc = setFreqNative(sRadioFD, frequency);
  if (rc < 0) {
    HAL_LOG("Could not set radio frequency");
  }

  sFrequency = frequency;

  NS_DispatchToMainThread(
      new RadioUpdate(hal::FM_RADIO_OPERATION_TUNE,
                      rc < 0 ? hal::FM_RADIO_OPERATION_STATUS_FAIL
                             : hal::FM_RADIO_OPERATION_STATUS_SUCCESS));
}

uint32_t GetFMRadioFrequency() {
  HAL_LOG("At %d In (GetFMRadioFrequency), sRadioEnabled %d", __LINE__, sRadioEnabled);
  if (!sRadioEnabled) return 0;

  HAL_LOG("GetFMRadioFrequency...");
  int rc = getFreqNative(sRadioFD);
  if (rc < 0) {
    HAL_LOG("Could not get radio frequency");
    return 0;
  }

  return rc;
}

bool IsFMRadioOn() { return sRadioEnabled; }

uint32_t GetFMRadioSignalStrength() {
  HAL_LOG("At %d In (GetFMRadioSignalStrength), sRadioEnabled %d", __LINE__, sRadioEnabled);
  if (!sRadioEnabled) return 0;

  int rmssiThreshold = getControlNative(sRadioFD, V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH);
  if (rmssiThreshold  == -1L) {
    HAL_LOG("Could not query fm radio for signal strength");
    return 0;
  }

  HAL_LOG("Signal Threshhold: %d", rmssiThreshold);

  int threshold = FM_RX_SIGNAL_STRENGTH_VERY_WEAK, signalStrength;

  if ((FM_RX_RSSI_LEVEL_VERY_WEAK < rmssiThreshold) && (rmssiThreshold <= FM_RX_RSSI_LEVEL_WEAK))
  {
    signalStrength = FM_RX_RSSI_LEVEL_WEAK;
  }
  else if ((FM_RX_RSSI_LEVEL_WEAK < rmssiThreshold) && (rmssiThreshold  <= FM_RX_RSSI_LEVEL_STRONG))
  {
    signalStrength = FM_RX_RSSI_LEVEL_STRONG;
  }
  else if ((FM_RX_RSSI_LEVEL_STRONG < rmssiThreshold))
  {
    signalStrength = FM_RX_RSSI_LEVEL_VERY_STRONG;
  }
  else
  {
    signalStrength = FM_RX_RSSI_LEVEL_VERY_WEAK;
  }
  //COPY from AOSP, NOT sure if the threshold is the desired signal strength.
  switch(signalStrength)
  {
  case FM_RX_RSSI_LEVEL_VERY_WEAK:
    threshold = FM_RX_SIGNAL_STRENGTH_VERY_WEAK;
    break;
  case FM_RX_RSSI_LEVEL_WEAK:
    threshold = FM_RX_SIGNAL_STRENGTH_WEAK;
    break;
  case FM_RX_RSSI_LEVEL_STRONG:
    threshold = FM_RX_SIGNAL_STRENGTH_STRONG;
    break;
  case FM_RX_RSSI_LEVEL_VERY_STRONG:
    threshold = FM_RX_SIGNAL_STRENGTH_VERY_STRONG;
    break;
  default:
    /* Should never reach here */
    break;
  }

  return threshold;
}

void CancelFMRadioSeek() {
  HAL_LOG("At %d In (CancelFMRadioSeek), sRadioEnabled %d", __LINE__, sRadioEnabled);
  int rc = cancelSearchNative(sRadioFD);
  if (rc < 0) HAL_LOG("Could not cancel hardware seek");
}

bool EnableRDS(uint32_t aMask) {
  HAL_LOG("At %d In (EnableRDS),sRadioEnabled: %d | sRDSSupported: %d", __LINE__, sRadioEnabled, sRDSSupported);
  if (!sRadioEnabled || !sRDSSupported) return false;

  int rc = setControlNative(sRadioFD, V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_MASK, aMask);
  if (rc < 0) {
    HAL_LOG("Could not set RDS Group Mask (%d)", rc);
  }

  if (sRDSEnabled) return true;

  rc = setControlNative(sRadioFD, V4L2_CID_PRIVATE_TAVARUA_RDSON, 1);
  if (rc < 0) {
    HAL_LOG("Could not enable RDS reception (%d)", rc);
    return false;
  }

  /* configure RT/PS/AF RDS processing */
  int rdsMask = FM_RX_RDS_GRP_RT_EBL |
                FM_RX_RDS_GRP_PS_EBL |
                FM_RX_RDS_GRP_AF_EBL |
                FM_RX_RDS_GRP_PS_SIMPLE_EBL |
                FM_RX_RDS_GRP_ECC_EBL |
                FM_RX_RDS_GRP_PTYN_EBL |
                FM_RX_RDS_GRP_RT_PLUS_EBL;

  int rds_group_mask = getControlNative(sRadioFD, V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_PROC);
  if (rds_group_mask == -1L) {
    HAL_LOG("Could not get RDS group mask (%d)", rc);
    return false;
  }
  //unsigned char rdsFilt = 0;
  int psAllVal = rdsMask & RDS_PS_ALL;
  HAL_LOG("In rdsOptions: rdsMask: %d", rdsMask);

  rds_group_mask = (rdsMask & 0x000000FF); //Copy from AOSP

  HAL_LOG("In rdsOptions: rds_group_mask : " + rds_group_mask);
  rc = setControlNative(sRadioFD, V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_PROC, rds_group_mask);
  if (rc < 0) {
    HAL_LOG("Could not set RDS rdsOptions (%d)", rc);
    return false;
  }

  sRDSEnabled = true;

  return true;
}

void DisableRDS() {
  HAL_LOG("At %d In (DisableRDS),sRadioEnabled: %d | sRDSSupported: %d", __LINE__, sRadioEnabled, sRDSSupported);
  if (!sRadioEnabled || !sRDSEnabled) return;

  int rc = rc = setControlNative(sRadioFD, V4L2_CID_PRIVATE_TAVARUA_RDSON, 0);
  if (rc < 0) {
    HAL_LOG("Could not disable RDS reception (%d)", rc);
  }

  sRDSEnabled = false;
}

}  // namespace hal_impl
}  // namespace mozilla
