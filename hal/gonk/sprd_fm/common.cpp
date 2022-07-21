/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fmr.h"

#ifdef LOG_TAG
#  undef LOG_TAG
#endif
#define LOG_TAG "FMLIB_COM"

int COM_open_dev(const char* pname, int* fd) {
  int ret = 0;
  int tmp = -1;

  FMR_ASSERT(pname);
  FMR_ASSERT(fd);

  LOGI("COM_open_dev start\n");
  tmp = open(pname, O_RDWR);
  if (tmp < 0) {
    LOGE("Open %s failed, %s\n", pname, strerror(errno));
    ret = -ERR_INVALID_FD;
  }
  *fd = tmp;
  LOGI("%s, [fd=%d] [ret=%d]\n", __func__, *fd, ret);
  return ret;
}

int COM_close_dev(int fd) {
  int ret = 0;

  LOGI("COM_close_dev start\n");
  ret = close(fd);
  if (ret) {
    LOGE("%s, failed\n", __func__);
  }
  LOGD("%s, [fd=%d] [ret=%d]\n", __func__, fd, ret);
  return ret;
}

int COM_pwr_up(int fd, int band, int freq) {
  int ret = 0;
  struct fm_tune_parm parm;

  LOGI("%s, [freq=%d]\n", __func__, freq);
  bzero(&parm, sizeof(struct fm_tune_parm));

  parm.band = band;
  parm.freq = freq;
  parm.hilo = FM_AUTO_HILO_OFF;
  parm.space = FM_SEEK_SPACE;

  ret = ioctl(fd, FM_IOCTL_POWERUP, &parm);
  if (ret) {
    LOGE("%s, failed\n", __func__);
  }
  LOGD("%s, [fd=%d] [ret=%d]\n", __func__, fd, ret);
  return ret;
}

int COM_pwr_down(int fd, int type) {
  int ret = 0;
  LOGI("%s, [type=%d]\n", __func__, type);
  ret = ioctl(fd, FM_IOCTL_POWERDOWN, &type);
  if (ret) {
    LOGE("%s, failed\n", __func__);
  }
  LOGD("%s, [fd=%d] [ret=%d]\n", __func__, fd, ret);
  return ret;
}

/* 0x20: space, 0x7E:~ */
#define ISVALID(c) ((c) >= 0x20 && (c) <= 0x7E)

/* change any char which out of [0x20,0x7E]to space(0x20) */
void COM_change_string(uint8_t* str, int len) {
  int i = 0;
  for (i = 0; i < len; i++) {
    if (false == ISVALID(str[i])) {
      str[i] = 0x20;
    }
  }
}

int COM_get_ps(int fd, RDSData_Struct* rds, uint8_t** ps, int* ps_len) {
  int ret = 0;
  char tmp_ps[9] = {0};

  FMR_ASSERT(rds);
  FMR_ASSERT(ps);
  FMR_ASSERT(ps_len);

  if (rds->event_status & RDS_EVENT_PROGRAMNAME) {
    LOGD("%s, Success,[event_status=%d]\n", __func__, rds->event_status);
    *ps = &rds->PS_Data.PS[3][0];
    *ps_len = sizeof(rds->PS_Data.PS[3]);

    COM_change_string(*ps, *ps_len);
    memcpy(tmp_ps, *ps, 8);
    LOGI("PS=%s\n", tmp_ps);
  } else {
    LOGE("%s, Failed,[event_status=%d]\n", __func__, rds->event_status);
    *ps = NULL;
    *ps_len = 0;
    ret = -ERR_RDS_NO_DATA;
  }

  return ret;
}

int COM_get_rt(int fd, RDSData_Struct* rds, uint8_t** rt, int* rt_len) {
  int ret = 0;
  char tmp_rt[65] = {0};

  FMR_ASSERT(rds);
  FMR_ASSERT(rt);
  FMR_ASSERT(rt_len);

  if (rds->event_status & RDS_EVENT_LAST_RADIOTEXT) {
    LOGD("%s, Success,[event_status=%d]\n", __func__, rds->event_status);
    *rt = &rds->RT_Data.TextData[3][0];
    *rt_len = rds->RT_Data.TextLength;

    COM_change_string(*rt, *rt_len);
    memcpy(tmp_rt, *rt, 64);
    LOGI("RT=%s\n", tmp_rt);
  } else {
    LOGE("%s, Failed,[event_status=%d]\n", __func__, rds->event_status);
    *rt = NULL;
    *rt_len = 0;
    ret = -ERR_RDS_NO_DATA;
  }
  return ret;
}

int COM_get_pi(int fd, RDSData_Struct* rds, uint16_t* pi) {
  int ret = 0;

  FMR_ASSERT(rds);
  FMR_ASSERT(pi);

  if (rds->event_status & RDS_EVENT_PI_CODE) {
    LOGD("%s, Success,[event_status=%d] [PI=%d]\n", __func__, rds->event_status,
         rds->PI);
    *pi = rds->PI;
  } else {
    LOGI("%s, Failed, there's no pi,[event_status=%d]\n", __func__,
         rds->event_status);
    *pi = -1;
    ret = -ERR_RDS_NO_DATA;
  }

  return ret;
}

int COM_tune(int fd, int freq, int band) {
  int ret = 0;

  struct fm_tune_parm parm;

  bzero(&parm, sizeof(struct fm_tune_parm));

  parm.band = band;
  parm.freq = freq;
  parm.hilo = FM_AUTO_HILO_OFF;
  parm.space = FM_SEEK_SPACE;

  ret = ioctl(fd, FM_IOCTL_TUNE, &parm);
  if (ret) {
    LOGE("%s, failed\n", __func__);
  }
  LOGD("%s, [fd=%d] [freq=%d] [ret=%d]\n", __func__, fd, freq, ret);
  return ret;
}

int COM_seek(int fd, int* freq, int band, int dir, int lev) {
  int ret = 0;
  struct fm_seek_parm parm;

  bzero(&parm, sizeof(struct fm_tune_parm));

  parm.band = band;
  parm.freq = *freq;
  parm.hilo = FM_AUTO_HILO_OFF;
  parm.space = FM_SEEK_SPACE;
  if (dir == 1) {
    parm.seekdir = FM_SEEK_UP;
  } else if (dir == 0) {
    parm.seekdir = FM_SEEK_DOWN;
  }
  parm.seekth = lev;

  ret = ioctl(fd, FM_IOCTL_SEEK, &parm);
  if (ret == 0) {
    *freq = parm.freq;
  }
  LOGD("%s, [fd=%d] [ret=%d] [freq=%d]\n", __func__, fd, ret, parm.freq);
  return ret;
}

int COM_set_mute(int fd, int mute) {
  int ret = 0;
  int tmp = mute;

  LOGD("%s, start \n", __func__);
  ret = ioctl(fd, FM_IOCTL_MUTE, &tmp);
  if (ret) {
    LOGE("%s, failed\n", __func__);
  }
  LOGD("%s, [fd=%d] [ret=%d]\n", __func__, fd, ret);
  return ret;
}

int COM_stop_scan(int fd) {
  int ret = 0;

  LOGD("%s, start \n", __func__);
  ret = ioctl(fd, FM_IOCTL_STOP_SCAN);
  if (ret) {
    LOGE("%s, failed\n", __func__);
  }
  LOGD("%s, [fd=%d] [ret=%d]\n", __func__, fd, ret);
  return ret;
}

int COM_is_fm_pwrup(int fd, int* pwrup) {
  int ret = 0;

  ret = ioctl(fd, FM_IOCTL_IS_FM_POWERED_UP, pwrup);
  if (ret) {
    LOGE("%s, failed\n", __func__);
  }
  LOGD("%s, [fd=%d] [ret=%d]\n", __func__, fd, ret);
  return ret;
}

/******************************************
 * Inquiry if RDS is support in driver.
 * Parameter:
 *      None
 * supt Value:
 *      1: support
 *      0: NOT support
 *      -1: error
 ******************************************/
int COM_is_rdsrx_support(int fd, int* supt) {
  int ret = 0;
  int support = -1;

  if (fd < 0) {
    LOGE("FM isRDSsupport fail, g_fm_fd = %d\n", fd);
    *supt = -1;
    ret = -ERR_INVALID_FD;
    return ret;
  }

  ret = ioctl(fd, FM_IOCTL_RDS_SUPPORT, &support);
  if (ret) {
    LOGE("FM FM_IOCTL_RDS_SUPPORT fail, errno = %d\n", errno);
    // don't support
    *supt = 0;
    return ret;
  }
  LOGI("isRDSsupport Success,[support=%d]\n", support);
  *supt = support;
  return ret;
}

int COM_pre_search(int fd) {
  fm_s32 ret = 0;
  ret = ioctl(fd, FM_IOCTL_PRE_SEARCH, 0);
  LOGD("COM_pre_search:%d\n", ret);
  return ret;
}

int COM_restore_search(int fd) {
  fm_s32 ret = 0;
  ret = ioctl(fd, FM_IOCTL_RESTORE_SEARCH, 0);
  LOGD("COM_restore_search:%d\n", ret);
  return ret;
}

/* soft mute tune function, usually for sw scan implement or CQI log tool */
int COM_Soft_Mute_Tune(int fd, fm_softmute_tune_t* para) {
  fm_s32 ret = 0;
  fm_softmute_tune_t value;

  value.freq = para->freq;
  ret = ioctl(fd, FM_IOCTL_SOFT_MUTE_TUNE, &value);
  if (ret) {
    LOGE("FM soft mute tune faild:%d\n", ret);
    return ret;
  }
  para->valid = value.valid;
  para->rssi = value.rssi;
  return 0;
}

int COM_get_cqi(int fd, int num, char* buf, int buf_len) {
  int ret;
  struct fm_cqi_req cqi_req;

  // check buf
  num = (num > CQI_CH_NUM_MAX) ? CQI_CH_NUM_MAX : num;
  num = (num < CQI_CH_NUM_MIN) ? CQI_CH_NUM_MIN : num;
  cqi_req.ch_num = (uint16_t)num;
  cqi_req.buf_size = cqi_req.ch_num * sizeof(struct fm_cqi);
  if (!buf || (buf_len < cqi_req.buf_size)) {
    LOGE("get cqi, invalid buf\n");
    return -1;
  }
  cqi_req.cqi_buf = buf;

  // get cqi from driver
  ret = ioctl(fd, FM_IOCTL_CQI_GET, &cqi_req);
  if (ret < 0) {
    LOGE("get cqi, failed %d\n", ret);
    return -1;
  }

  return 0;
}

int COM_turn_on_off_rds(int fd, int onoff) {
  int ret = 0;
  uint16_t rds_on = -1;

  LOGD("Rdsset start\n");
  if (onoff == FMR_RDS_ON) {
    rds_on = 1;
    ret = ioctl(fd, FM_IOCTL_RDS_ONOFF, &rds_on);
    if (ret) {
      LOGE("FM_IOCTL_RDS_ON failed\n");
      return ret;
    }
    LOGD("Rdsset Success,[rds_on=%d]\n", rds_on);
  } else {
    rds_on = 0;
    ret = ioctl(fd, FM_IOCTL_RDS_ONOFF, &rds_on);
    if (ret) {
      LOGE("FM_IOCTL_RDS_OFF failed\n");
      return ret;
    }
    LOGD("Rdsset Success,[rds_on=%d]\n", rds_on);
  }
  return ret;
}

int COM_get_chip_id(int fd, int* chipid) {
  int ret = 0;
  uint16_t tmp = 0;

  FMR_ASSERT(chipid);

  ret = ioctl(fd, FM_IOCTL_GETCHIPID, &tmp);
  *chipid = (int)tmp;
  if (ret) {
    LOGE("%s, failed\n", __func__);
  }
  LOGD("%s, [fd=%d] [chipid=%x] [ret=%d]\n", __func__, fd, *chipid, ret);
  return ret;
}

int COM_get_rssi(int fd, int* rssi) {
  int ret = 0;
  int32_t tmp = 0;

  FMR_ASSERT(rssi);

  ret = ioctl(fd, FM_IOCTL_GETRSSI, &tmp);
  *rssi = (int)tmp;
  if (ret) {
    LOGE("%s, failed\n", __func__);
  }
  LOGD("%s, [fd=%d] [rssi=%x] [ret=%d]\n", __func__, fd, *rssi, ret);
  return ret;
}

int COM_get_bler(int fd, int* bler) {
  int ret = 0;
  int32_t tmp = 0;

  FMR_ASSERT(bler);

  ret = ioctl(fd, FM_IOCTL_GET_CUR_BLER_CMD, &tmp);
  *bler = (int)tmp;

  if (ret) {
    LOGE("%s, failed\n", __func__);
  }

  LOGD("%s, [fd=%d] [bler=%x] [ret=%d]\n", __func__, fd, *bler, ret);

  return ret;
}

int COM_read_rds_data(int fd, RDSData_Struct* rds, uint16_t* rds_status) {
  int ret = 0;
  uint16_t event_status;

  FMR_ASSERT(rds);
  FMR_ASSERT(rds_status);

  if (read(fd, rds, sizeof(RDSData_Struct)) == sizeof(RDSData_Struct)) {
    event_status = rds->event_status;
    LOGI("event_status = 0x%x\n", event_status);
    *rds_status = event_status;
    return ret;
  } else {
    ret = -ERR_RDS_NO_DATA;
  }
  return ret;
}

int COM_active_af(int fd, RDSData_Struct* rds, int band, uint16_t cur_freq,
                  uint16_t* ret_freq) {
  int ret = 0;
  int i = 0;
  struct fm_tune_parm parm;
  uint16_t rds_on = 0;
  uint16_t set_freq, sw_freq, org_freq, PAMD_Value, AF_PAMD_LBound,
      AF_PAMD_HBound;
  uint16_t PAMD_Level[25];
  uint16_t PAMD_DB_TBL[5] = {// 5dB, 10dB, 15dB, 20dB, 25dB,
                             //  13, 17, 21, 25, 29};
                             8, 12, 15, 18, 20};
  FMR_ASSERT(rds);
  FMR_ASSERT(ret_freq);

  sw_freq = cur_freq;  // current freq
  org_freq = cur_freq;
  parm.band = band;
  parm.freq = sw_freq;
  parm.hilo = FM_AUTO_HILO_OFF;
  parm.space = FM_SPACE_DEFAULT;

  if (!(rds->event_status & RDS_EVENT_AF)) {
    LOGE("activeAF failed\n");
    *ret_freq = 0;
    ret = -ERR_RDS_NO_DATA;
    return ret;
  }

  AF_PAMD_LBound = PAMD_DB_TBL[0];  // 5dB
  AF_PAMD_HBound = PAMD_DB_TBL[1];  // 15dB
  ioctl(fd, FM_IOCTL_GETCURPAMD, &PAMD_Value);
  LOGI("current_freq=%d,PAMD_Value=%d\n", cur_freq, PAMD_Value);

  if (PAMD_Value < AF_PAMD_LBound) {
    rds_on = 0;
    ioctl(fd, FM_IOCTL_RDS_ONOFF, &rds_on);
    // make sure rds->AF_Data.AF_Num is valid
    rds->AF_Data.AF_Num = (rds->AF_Data.AF_Num > 25) ? 25 : rds->AF_Data.AF_Num;
    for (i = 0; i < rds->AF_Data.AF_Num; i++) {
      set_freq = rds->AF_Data.AF[1][i];  // method A or B
      if (set_freq != org_freq) {
        parm.freq = set_freq;
        ioctl(fd, FM_IOCTL_TUNE, &parm);
        usleep(250 * 1000);
        ioctl(fd, FM_IOCTL_GETCURPAMD, &PAMD_Level[i]);
        LOGI("next_freq=%d,PAMD_Level=%d\n", parm.freq, PAMD_Level[i]);
        if (PAMD_Level[i] > PAMD_Value) {
          PAMD_Value = PAMD_Level[i];
          sw_freq = set_freq;
        }
      }
    }
    LOGI("PAMD_Value=%d, sw_freq=%d\n", PAMD_Value, sw_freq);
    if ((PAMD_Value > AF_PAMD_HBound) && (sw_freq != 0)) {
      parm.freq = sw_freq;
      ioctl(fd, FM_IOCTL_TUNE, &parm);
      cur_freq = parm.freq;
    } else {
      parm.freq = org_freq;
      ioctl(fd, FM_IOCTL_TUNE, &parm);
      cur_freq = parm.freq;
    }
    rds_on = 1;
    ioctl(fd, FM_IOCTL_RDS_ONOFF, &rds_on);
  } else {
    LOGD("RDS_EVENT_AF old freq:%d\n", org_freq);
    ret = -1;
  }
  *ret_freq = cur_freq;

  return ret;
}

int COM_ana_switch(int fd, int antenna) {
  int ret = 0;

  ret = ioctl(fd, FM_IOCTL_ANA_SWITCH, &antenna);
  if (ret < 0) {
    LOGE("%s: fail, ret = %d\n", __func__, ret);
  }

  LOGD("%s: fd %d,antenna %d,ret %d\n", __func__, fd, antenna, ret);
  return ret;
}

int COM_ana_switch_inner(int fd, int antenna) {
  int ret = 0;

  ret = ioctl(fd, FM_IOCTL_ANA_SWITCH_INNER, &antenna);
  if (ret < 0) {
    LOGE("%s: fail, ret = %d\n", __func__, ret);
  }

  LOGD("%s: [ret = %d]\n", __func__, ret);
  return ret;
}

/*  COM_is_dese_chan -- check if gived channel is a de-sense channel or not
 *  @fd - fd of "dev/fm"
 *  @freq - gived channel
 *  return value: 0, not a dese chan; 1, a dese chan; else error NO.
 */
int COM_is_dese_chan(int fd, int freq) {
  int ret = 0;
  int tmp = freq;

  ret = ioctl(fd, FM_IOCTL_IS_DESE_CHAN, &freq);
  if (ret < 0) {
    LOGE("%s, failed,ret=%d\n", __func__, ret);
    return ret;
  } else {
    LOGD("[fd=%d] %d --> dese=%d\n", fd, tmp, freq);
    return freq;
  }
}

/*  COM_desense_check -- check if gived channel is a de-sense channel or not
 *  @fd - fd of "dev/fm"
 *  @freq - gived channel
 *  @rssi-freq's rssi
 *  return value: 0, is desense channel and rssi is less than threshold; 1, not
 *                desense channel or it is but rssi is more than threshold.
 */
int COM_desense_check(int fd, int freq, int rssi) {
  int ret = 0;
  fm_desense_check_t parm;

  parm.freq = freq;
  parm.rssi = rssi;
  ret = ioctl(fd, FM_IOCTL_DESENSE_CHECK, &parm);
  if (ret < 0) {
    LOGE("%s, failed,ret=%d\n", __func__, ret);
    return ret;
  } else {
    LOGD("[fd=%d] %d --> dese=%d\n", fd, freq, ret);
    return ret;
  }
}

int COM_set_dcc_bw(int fd, int bw) {
  int ret = 0;

  LOGD("%s, start \n", __func__);
  ret = ioctl(fd, FM_IOCTL_DCC_SETBW, &bw);
  if (ret) {
    LOGE("%s, failed\n", __func__);
  }
  LOGD("%s, [fd=%d] [ret=%d]\n", __func__, fd, ret);

  return ret;
}

static int COM_set_vol(int fd, int vol) {
  int ret = 0;

  LOGD("%s, start \n", __func__);
  ret = ioctl(fd, FM_IOCTL_SETVOL, &vol);
  if (ret) {
    LOGE("%s, failed\n", __func__);
  }
  LOGD("%s, [fd=%d] [ret=%d]\n", __func__, fd, ret);

  return ret;
}

int COM_get_vol(int fd, int* vol) {
  int ret = 0;
  int32_t tmp = 0;

  FMR_ASSERT(vol);

  ret = ioctl(fd, FM_IOCTL_GETVOL, &tmp);
  *vol = (int)tmp;
  if (ret) {
    LOGE("%s, failed\n", __func__);
  }
  LOGD("%s, [fd=%d] [vol=%x] [ret=%d]\n", __func__, fd, *vol, ret);

  return ret;
}

void FM_interface_init(struct fm_cbk_tbl* cbk_tbl) {
  // Basic functions.
  cbk_tbl->open_dev = COM_open_dev;
  cbk_tbl->close_dev = COM_close_dev;
  cbk_tbl->pwr_up = COM_pwr_up;
  cbk_tbl->pwr_down = COM_pwr_down;
  cbk_tbl->seek = COM_seek;
  cbk_tbl->tune = COM_tune;
  cbk_tbl->set_mute = COM_set_mute;
  cbk_tbl->stop_scan = COM_stop_scan;
  cbk_tbl->is_rdsrx_support = COM_is_rdsrx_support;
  cbk_tbl->turn_on_off_rds = COM_turn_on_off_rds;
  cbk_tbl->get_chip_id = COM_get_chip_id;
  cbk_tbl->get_rssi = COM_get_rssi;
  cbk_tbl->get_bler = COM_get_bler;
  // For RDS RX.
  cbk_tbl->read_rds_data = COM_read_rds_data;
  cbk_tbl->get_ps = COM_get_ps;
  cbk_tbl->get_rt = COM_get_rt;
  cbk_tbl->active_af = COM_active_af;
  // FM short antenna
  cbk_tbl->ana_switch = COM_ana_switch;
  cbk_tbl->ana_switch_inner = COM_ana_switch_inner;
  cbk_tbl->desense_check = COM_desense_check;
  // soft mute tune
  cbk_tbl->soft_mute_tune = COM_Soft_Mute_Tune;
  cbk_tbl->pre_search = COM_pre_search;
  cbk_tbl->restore_search = COM_restore_search;
  cbk_tbl->set_dcc_bw = COM_set_dcc_bw;
  cbk_tbl->set_vol = COM_set_vol;
  cbk_tbl->get_vol = COM_get_vol;
  return;
}
