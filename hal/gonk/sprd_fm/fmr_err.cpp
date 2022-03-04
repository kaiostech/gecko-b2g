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
#define LOG_TAG "FMLIB_ERR"

#define FMR_ERR_BASE -0x100

static int fmr_err = 0;
static char tmp[20] = {0};

char* FMR_strerr() {
  snprintf(tmp, sizeof(tmp), "%d", fmr_err);

  return tmp;
}

void FMR_seterr(int err) { fmr_err = err; }
