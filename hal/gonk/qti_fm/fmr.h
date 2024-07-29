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

#ifndef __FMR_H__
#define __FMR_H__

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <utils/Log.h>
#include <utils/misc.h>
#include <cutils/properties.h>
#include <fcntl.h>
#include <math.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <assert.h>
#include <dlfcn.h>
#include <vector>

typedef unsigned int UINT;
typedef unsigned long ULINT;

#define FM_JNI_SUCCESS 0L
#define FM_JNI_FAILURE -1L

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

/* V4l2 Controls */
enum v4l2_cid_private_tavarua_t
{
  V4L2_CID_PRIVATE_TAVARUA_SRCHMODE              = V4L2_CID_PRIVATE_BASE + 1,
  V4L2_CID_PRIVATE_TAVARUA_SCANDWELL             = V4L2_CID_PRIVATE_BASE + 2,
  V4L2_CID_PRIVATE_TAVARUA_SRCHON                = V4L2_CID_PRIVATE_BASE + 3,
  V4L2_CID_PRIVATE_TAVARUA_STATE                 = V4L2_CID_PRIVATE_BASE + 4,
  V4L2_CID_PRIVATE_TAVARUA_TRANSMIT_MODE         = V4L2_CID_PRIVATE_BASE + 5,
  V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_MASK         = V4L2_CID_PRIVATE_BASE + 6,
  V4L2_CID_PRIVATE_TAVARUA_REGION                = V4L2_CID_PRIVATE_BASE + 7,
  V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH             = V4L2_CID_PRIVATE_BASE + 8,
  V4L2_CID_PRIVATE_TAVARUA_SRCH_PTY              = V4L2_CID_PRIVATE_BASE + 9,
  V4L2_CID_PRIVATE_TAVARUA_SRCH_PI               = V4L2_CID_PRIVATE_BASE + 10,
  V4L2_CID_PRIVATE_TAVARUA_SRCH_CNT              = V4L2_CID_PRIVATE_BASE + 11,
  V4L2_CID_PRIVATE_TAVARUA_EMPHASIS              = V4L2_CID_PRIVATE_BASE + 12,
  V4L2_CID_PRIVATE_TAVARUA_RDS_STD               = V4L2_CID_PRIVATE_BASE + 13,
  V4L2_CID_PRIVATE_TAVARUA_SPACING               = V4L2_CID_PRIVATE_BASE + 14,
  V4L2_CID_PRIVATE_TAVARUA_RDSON                 = V4L2_CID_PRIVATE_BASE + 15,
  V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_PROC         = V4L2_CID_PRIVATE_BASE + 16,
  V4L2_CID_PRIVATE_TAVARUA_LP_MODE               = V4L2_CID_PRIVATE_BASE + 17,
  V4L2_CID_PRIVATE_TAVARUA_RDSD_BUF              = V4L2_CID_PRIVATE_BASE + 19,
  V4L2_CID_PRIVATE_TAVARUA_IOVERC                = V4L2_CID_PRIVATE_BASE + 24,
  V4L2_CID_PRIVATE_TAVARUA_INTDET                = V4L2_CID_PRIVATE_BASE + 25,
  V4L2_CID_PRIVATE_TAVARUA_MPX_DCC               = V4L2_CID_PRIVATE_BASE + 26,
  V4L2_CID_PRIVATE_TAVARUA_AF_JUMP               = V4L2_CID_PRIVATE_BASE + 27,
  V4L2_CID_PRIVATE_TAVARUA_RSSI_DELTA            = V4L2_CID_PRIVATE_BASE + 28,
  V4L2_CID_PRIVATE_TAVARUA_HLSI                  = V4L2_CID_PRIVATE_BASE + 29,
  V4L2_CID_PRIVATE_TAVARUA_SET_AUDIO_PATH        = V4L2_CID_PRIVATE_BASE + 41,
  V4L2_CID_PRIVATE_SINR                          = V4L2_CID_PRIVATE_BASE + 44,
  V4L2_CID_PRIVATE_TAVARUA_ON_CHANNEL_THRESHOLD  = V4L2_CID_PRIVATE_BASE + 0x2D,
  V4L2_CID_PRIVATE_TAVARUA_OFF_CHANNEL_THRESHOLD = V4L2_CID_PRIVATE_BASE + 0x2E,
  V4L2_CID_PRIVATE_TAVARUA_SINR_THRESHOLD        = V4L2_CID_PRIVATE_BASE + 0x2F,
  V4L2_CID_PRIVATE_TAVARUA_SINR_SAMPLES          = V4L2_CID_PRIVATE_BASE + 0x30,
  V4L2_CID_PRIVATE_SPUR_FREQ                     = V4L2_CID_PRIVATE_BASE + 0x31,
  V4L2_CID_PRIVATE_SPUR_FREQ_RMSSI               = V4L2_CID_PRIVATE_BASE + 0x32,
  V4L2_CID_PRIVATE_SPUR_SELECTION                = V4L2_CID_PRIVATE_BASE + 0x33,
  V4L2_CID_PRIVATE_AF_RMSSI_TH                   = V4L2_CID_PRIVATE_BASE + 0x36,
  V4L2_CID_PRIVATE_AF_RMSSI_SAMPLES              = V4L2_CID_PRIVATE_BASE + 0x37,
  V4L2_CID_PRIVATE_GOOD_CH_RMSSI_TH              = V4L2_CID_PRIVATE_BASE + 0x38,
  V4L2_CID_PRIVATE_SRCHALGOTYPE                  = V4L2_CID_PRIVATE_BASE + 0x39,
  V4L2_CID_PRIVATE_CF0TH12                       = V4L2_CID_PRIVATE_BASE + 0x3A,
  V4L2_CID_PRIVATE_SINRFIRSTSTAGE                = V4L2_CID_PRIVATE_BASE + 0x3B,
  V4L2_CID_PRIVATE_RMSSIFIRSTSTAGE               = V4L2_CID_PRIVATE_BASE + 0x3C,
  V4L2_CID_PRIVATE_RXREPEATCOUNT                 = V4L2_CID_PRIVATE_BASE + 0x3D,
  V4L2_CID_PRIVATE_RSSI_TH                       = V4L2_CID_PRIVATE_BASE + 0x3E,
  V4L2_CID_PRIVATE_AF_JUMP_RSSI_TH               = V4L2_CID_PRIVATE_BASE + 0x3F,
  V4L2_CID_PRIVATE_BLEND_SINRHI                  = V4L2_CID_PRIVATE_BASE + 0x40,
  V4L2_CID_PRIVATE_BLEND_RMSSIHI                 = V4L2_CID_PRIVATE_BASE + 0x41,
  ENABLE_LOW_PASS_FILTER                         = V4L2_CID_PRIVATE_BASE + 0x45,
};

enum {
  // FM PROPERTY
  FM_PROP_NOTCH_VALUE = 0x1000,
  FM_PROP_HW_INIT,
  FM_PROP_HW_MODE,
  FM_PROP_CTL_START,
  FM_PROP_CTL_STOP,
  FM_STATS_PROP,
  FM_PROP_WAN_RATCONF,
  FM_PROP_BTWLAN_LPFENABLER,
};

enum radio_state_t {
    FM_OFF,
    FM_RECV,
    FM_TRANS,
    FM_RESET,
    FM_CALIB,
    FM_TURNING_OFF,
    FM_RECV_TURNING_ON,
    FM_TRANS_TURNING_ON,
    FM_MAX_NO_STATES,
};

enum channel_spacing {
  FM_CH_SPACE_200KHZ,
  FM_CH_SPACE_100KHZ,
  FM_CH_SPACE_50KHZ
};

enum audio_path {
  FM_DIGITAL_PATH,
  FM_ANALOG_PATH
};

enum search_dwell_t {
  FM_RX_DWELL_PERIOD_0S,
  FM_RX_DWELL_PERIOD_1S,
  FM_RX_DWELL_PERIOD_2S,
  FM_RX_DWELL_PERIOD_3S,
  FM_RX_DWELL_PERIOD_4S,
  FM_RX_DWELL_PERIOD_5S,
  FM_RX_DWELL_PERIOD_6S,
  FM_RX_DWELL_PERIOD_7S
};

enum search_mode_t {
    FM_RX_SRCH_MODE_SEEK,
    FM_RX_SRCH_MODE_SCAN
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

enum FM_RX_RSSI_LEVEL_t {
    FM_RX_RSSI_LEVEL_VERY_WEAK   = -105,
    FM_RX_RSSI_LEVEL_WEAK        = -100,
    FM_RX_RSSI_LEVEL_STRONG      = -96,
    FM_RX_RSSI_LEVEL_VERY_STRONG = -90,
};

enum FM_RX_SIGNAL_STRENGTH_t {
    FM_RX_SIGNAL_STRENGTH_VERY_WEAK,
    FM_RX_SIGNAL_STRENGTH_WEAK,
    FM_RX_SIGNAL_STRENGTH_STRONG,
    FM_RX_SIGNAL_STRENGTH_VERY_STRONG,
};

enum FM_RX_RDS_GRP_mask_t {
  FM_RX_RDS_GRP_RT_EBL         =1,
  FM_RX_RDS_GRP_PS_EBL         =2,
  FM_RX_RDS_GRP_PS_SIMPLE_EBL  =4,
  FM_RX_RDS_GRP_AF_EBL         =8,
  FM_RX_RDS_GRP_ECC_EBL        =32,
  FM_RX_RDS_GRP_PTYN_EBL       =64,
  FM_RX_RDS_GRP_RT_PLUS_EBL    =128,
};

enum RDS_GROUP_mask_t {
  RDS_GROUP_RT = 0x1,
  RDS_GROUP_PS = 1 << 1,
  RDS_GROUP_AF = 1 << 2,
  RDS_PS_ALL   = 1 << 4,
  RDS_AF_AUTO  = 1 << 6,
};

typedef void (*enb_result_cb)(void);
typedef void (*tune_rsp_cb)(int Freq);
typedef void (*seek_rsp_cb)(int Freq);
typedef void (*scan_rsp_cb)(void);
typedef void (*srch_list_rsp_cb)(uint16_t *scan_tbl);
typedef void (*stereo_mode_cb)(bool status);
typedef void (*rds_avl_sts_cb)(bool status);
typedef void (*af_list_cb)(uint16_t *af_list);
typedef void (*rt_cb)(char *rt);
typedef void (*ps_cb)(char *ps);
typedef void (*oda_cb)(void);
typedef void (*rt_plus_cb)(char *rt_plus);
typedef void (*ert_cb)(char *ert);
typedef void (*disable_cb)(void);
typedef void (*callback_thread_event)(unsigned int evt);
typedef void (*rds_grp_cntrs_cb)(char *rds_params);
typedef void (*rds_grp_cntrs_ext_cb)(char *rds_params);
typedef void (*fm_peek_cb)(char *peek_rsp);
typedef void (*fm_ssbi_peek_cb)(char *ssbi_peek_rsp);
typedef void (*fm_agc_gain_cb)(char *agc_gain_rsp);
typedef void (*fm_ch_det_th_cb)(char *ch_det_rsp);
typedef void (*fm_ecc_evt_cb)(char *ecc_rsp);
typedef void (*fm_sig_thr_cb) (int val, int status);
typedef void (*fm_get_ch_det_thrs_cb) (int val, int status);
typedef void (*fm_def_data_rd_cb) (int val, int status);
typedef void (*fm_get_blnd_cb) (int val, int status);
typedef void (*fm_set_ch_det_thrs_cb) (int status);
typedef void (*fm_def_data_wrt_cb) (int status);
typedef void (*fm_set_blnd_cb) (int status);
typedef void (*fm_get_stn_prm_cb) (int val, int status);
typedef void (*fm_get_stn_dbg_prm_cb) (int val, int status);
typedef void (*fm_enable_slimbus_cb) (int status);
typedef void (*fm_enable_softmute_cb) (int status);

typedef struct {
    size_t  size;

    enb_result_cb enabled_cb;
    tune_rsp_cb tune_cb;
    seek_rsp_cb seek_cmpl_cb;
    scan_rsp_cb scan_next_cb;
    srch_list_rsp_cb srch_list_cb;
    stereo_mode_cb stereo_status_cb;
    rds_avl_sts_cb rds_avail_status_cb;
    af_list_cb af_list_update_cb;
    rt_cb rt_update_cb;
    ps_cb ps_update_cb;
    oda_cb oda_update_cb;
    rt_plus_cb rt_plus_update_cb;
    ert_cb  ert_update_cb;
    disable_cb disabled_cb;
    rds_grp_cntrs_cb rds_grp_cntrs_rsp_cb;
    rds_grp_cntrs_ext_cb rds_grp_cntrs_ext_rsp_cb;
    fm_peek_cb fm_peek_rsp_cb;
    fm_ssbi_peek_cb fm_ssbi_peek_rsp_cb;
    fm_agc_gain_cb fm_agc_gain_rsp_cb;
    fm_ch_det_th_cb fm_ch_det_th_rsp_cb;
    fm_ecc_evt_cb ext_country_code_cb;
    callback_thread_event thread_evt_cb;
    fm_sig_thr_cb fm_get_sig_thres_cb;
    fm_get_ch_det_thrs_cb fm_get_ch_det_thr_cb;
    fm_def_data_rd_cb fm_def_data_read_cb;
    fm_get_blnd_cb fm_get_blend_cb;
    fm_set_ch_det_thrs_cb fm_set_ch_det_thr_cb;
    fm_def_data_wrt_cb fm_def_data_write_cb;
    fm_set_blnd_cb fm_set_blend_cb;
    fm_get_stn_prm_cb fm_get_station_param_cb;
    fm_get_stn_dbg_prm_cb fm_get_station_debug_param_cb;
    fm_enable_slimbus_cb enable_slimbus_cb;
    fm_enable_softmute_cb enable_softmute_cb;
} fm_hal_callbacks_t;

struct fm_interface_t {
    int (*hal_init)(const fm_hal_callbacks_t *p_cb);
    int (*set_fm_ctrl)(int opcode, int val);
    int (*get_fm_ctrl) (int opcode, int *val);
};

/****************** Function declaration ******************/
namespace mozilla {
namespace hal_impl {

void fm_enabled_cb(void);
void fm_tune_cb(int Freq);
void fm_seek_cmpl_cb(int Freq);
void fm_scan_next_cb(void);
void fm_srch_list_cb(uint16_t *scan_tbl);
void fm_stereo_status_cb(bool stereo);
void fm_rds_avail_status_cb(bool rds_avl);
void fm_af_list_update_cb(uint16_t *af_list);
void fm_rt_update_cb(char *rt);
void fm_ps_update_cb(char *ps);
void fm_oda_update_cb(void);
void fm_rt_plus_update_cb(char *rt_plus);
void fm_ert_update_cb(char *ert);
void fm_ext_country_code_cb(char *ecc);
void rds_grp_cntrs_rsp_cb(char * evt_buffer);
void rds_grp_cntrs_ext_rsp_cb(char * evt_buffer);
void fm_disabled_cb(void);
void fm_peek_rsp_cb(char *peek_rsp);
void fm_ssbi_peek_rsp_cb(char *ssbi_peek_rsp);
void fm_agc_gain_rsp_cb(char *agc_gain_rsp);
void fm_ch_det_th_rsp_cb(char *ch_det_rsp);
void fm_thread_evt_cb(unsigned int event);
void fm_get_sig_thres_cb(int val, int status);
void fm_get_ch_det_thr_cb(int val, int status);
void fm_set_ch_det_thr_cb(int status);
void fm_def_data_read_cb(int val, int status);
void fm_def_data_write_cb(int status);
void fm_get_blend_cb(int val, int status);
void fm_set_blend_cb(int status);
void fm_get_station_param_cb(int val, int status);
void fm_get_station_debug_param_cb(int val, int status);
void fm_enable_slimbus_cb(int status);
void fm_enable_softmute_cb(int status);

// fmr_core.cpp
void get_property(int ptype, char *value);
int getFreqNative(int fd);
int setFreqNative(int fd, int freq);
int setControlNative(int fd, int id, int value);
int getControlNative(int fd, int id);
int startSearchNative(int fd, int dir);
int cancelSearchNative(int fd);
int getRSSINative(int fd);
int setBandNative(int fd, int low, int high);
int getLowerBandNative(int fd);
int getUpperBandNative(int fd);
int setMonoStereoNative(int fd, int val);
int getRawRdsNative(int fd, std::vector<bool> buff, int count);
int configureSpurTable(int fd);
int setPSRepeatCountNative(int fd, int repCount);
int setTxPowerLevelNative(int fd, int powLevel);
int setSpurDataNative(int fd, std::vector<short> buff, int count);
int enableSlimbusNative(int fd, int val);
bool getFmStatsPropNative();
int getFmCoexPropNative(int fd, int prop);
int enableSoftMuteNative(int fd, int val);
void classInitNative();
void initNative();

}  // namespace hal_impl
}  // namespace mozilla

#endif  // __FMR_H__
