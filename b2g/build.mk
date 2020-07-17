# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(topsrcdir)/toolkit/mozapps/installer/package-name.mk

installer: 
	@$(MAKE) -C b2g/installer installer

package:
	@$(MAKE) -C b2g/installer

install::
	@adb shell stop b2g
	@adb remount
	@adb shell rm -rf /system/b2g
	@adb push dist/b2g/ /system/b2g
	@adb shell start b2g

upload::
	@$(MAKE) -C b2g/installer upload

ifdef ENABLE_TESTS
# Implemented in testing/testsuite-targets.mk

mochitest-browser-chrome:
	$(RUN_MOCHITEST) --browser-chrome
	$(CHECK_TEST_ERROR)

mochitest:: mochitest-browser-chrome

.PHONY: mochitest-browser-chrome
endif

