/* Copyright (C) 2020 KAI OS TECHNOLOGIES (HONG KONG) LIMITED. All rights
 * reserved. Copyright (C) 2015 Acadine Technologies. All rights reserved.
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

#include "rgb8888_to_rgb565_neon.h"

namespace mozilla {

#define RBGATORGB565                                                        \
  "vshr.u8    d20, d20, #3                   \n" /* R                    */ \
  "vshr.u8    d21, d21, #2                   \n" /* G                    */ \
  "vshr.u8    d22, d22, #3                   \n" /* B                    */ \
  "vmovl.u8   q8, d20                        \n" /* R                    */ \
  "vmovl.u8   q9, d21                        \n" /* G                    */ \
  "vmovl.u8   q10, d22                       \n" /* B                    */ \
  "vshl.u16   q8, q8, #11                    \n" /* R                    */ \
  "vshl.u16   q9, q9, #5                     \n" /* G                    */ \
  "vorr       q0, q8, q9                     \n" /* RG                   */ \
  "vorr       q0, q0, q10                    \n" /* RGB                  */

void Transform8888To565_NEON(uint8_t* outbuf, const uint8_t* inbuf,
                             int PixelNum) {
  asm volatile(
      ".p2align   2                              \n"
      "1:                                          \n"
      "vld4.8     {d20, d21, d22, d23}, [%0]!    \n"  // load 8 pixels of ARGB.
      "subs       %2, %2, #8                     \n"  // 8 processed per loop.
      RBGATORGB565
      "vst1.8     {q0}, [%1]!                    \n"  // store 8 pixels RGB565.
      "bgt        1b                             \n"
      : "+r"(inbuf),    // %0
        "+r"(outbuf),   // %1
        "+r"(PixelNum)  // %2
      :
      : "cc", "memory", "q0", "q8", "q9", "q10");
}
}  // namespace mozilla
