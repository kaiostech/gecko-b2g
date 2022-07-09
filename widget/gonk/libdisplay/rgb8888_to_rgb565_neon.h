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

#ifndef RGB8888_TO_RGB565_NEON_H
#define RGB8888_TO_RGB565_NEON_H

#include <stdint.h>

namespace mozilla {

void Transform8888To565_NEON(uint8_t* outbuf, const uint8_t* inbuf,
                             int PixelNum);

}
#endif /* RGB8888_TO_RGB565_NEON_H */
