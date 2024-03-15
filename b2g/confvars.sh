# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

MOZ_APP_VENDOR=${PRODUCTION_OS_NAME-"B2GOS"}

MOZ_APP_VERSION=4.0
MOZ_APP_UA_NAME=${PRODUCTION_OS_NAME-"B2GOS"}

MOZ_UA_OS_AGNOSTIC=1

MOZ_BRANDING_DIRECTORY=b2g/branding/unofficial
MOZ_OFFICIAL_BRANDING_DIRECTORY=b2g/branding/official
# MOZ_APP_DISPLAYNAME is set by branding/configure.sh

MOZ_SAFE_BROWSING=1
MOZ_SERVICES_COMMON=1

MOZ_WEBSMS_BACKEND=1
MOZ_NO_SMART_CARDS=1
MOZ_APP_STATIC_INI=1
NSS_DISABLE_DBM=1
MOZ_NO_EV_CERTS=1

MOZ_WEBSPEECH=1
MOZ_WEBSPEECH_TEST_BACKEND=1

if test "$OS_TARGET" = "Android"; then
MOZ_RAW=1
MOZ_AUDIO_CHANNEL_MANAGER=1
fi

# use custom widget for html:select
MOZ_USE_NATIVE_POPUP_WINDOWS=1

MOZ_APP_ID={3c2e2abc-06d4-11e1-ac3b-374f68613e61}

MOZ_TIME_MANAGER=1

MOZ_PLACES=
MOZ_B2G=1

if test "$OS_TARGET" = "Android"; then
MOZ_ENABLE_WARNINGS_AS_ERRORS=1
fi

MOZ_JSDOWNLOADS=1

MOZ_BUNDLED_FONTS=1

BROWSER_CHROME_URL=chrome://b2g/content/shell.html

export JS_GC_SMALL_CHUNK_SIZE=1
