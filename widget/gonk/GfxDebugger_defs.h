/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et ft=cpp: tw=80: */

/* (c) 2017 KAI OS TECHNOLOGIES (HONG KONG) LIMITED All rights reserved. This
 * file or any portion thereof may not be reproduced or used in any manner
 * whatsoever without the express written permission of KAI OS TECHNOLOGIES
 * (HONG KONG) LIMITED. KaiOS is the trademark of KAI OS TECHNOLOGIES (HONG
 * KONG) LIMITED or its affiliate company and may be registered in some
 * jurisdictions. All other trademarks are the property of their respective
 * owners.
 */

#ifndef GFX_DEBUGGER_DEFS_H
#define GFX_DEBUGGER_DEFS_H

enum {
  GD_CMD_GRALLOC,
  GD_CMD_SCREENCAP,
  GD_CMD_SCREENRECORD,
  GD_CMD_LAYER,
  GD_CMD_APZ,

  GD_ERR,
};

enum {
  GRALLOC_OP_LIST,
  GRALLOC_OP_DUMP,
  SCREENCAP_OP_CAPTURE,
  SCREENRECORD_OP_CAPTURE,

  OP_ERR,
};

enum {
  FORMAT_MP4,
  FORMAT_WEBM,
  FORMAT_3GPP,
};

#endif /* GFX_DEBUGGER_DEFS_H */
