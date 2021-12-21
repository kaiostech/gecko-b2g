/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#filter substitution

pref("toolkit.defaultChromeURI", "chrome://b2g/content/shell.html");
pref("browser.chromeURL", "chrome://b2g/content/");

// Bug 945235: Prevent all bars to be considered visible:
pref("toolkit.defaultChromeFeatures", "chrome,dialog=no,close,resizable,scrollbars,extrachrome,mozdisplayid=0");

// Disable focus rings
pref("browser.display.focus_ring_width", 0);

/* cache prefs */
#ifdef MOZ_WIDGET_GONK
pref("browser.cache.disk.enable", true);
pref("browser.cache.disk.capacity", 55000); // kilobytes
pref("browser.cache.disk.parent_directory", "/cache");
#endif
pref("browser.cache.disk.smart_size.enabled", false);
pref("browser.cache.disk.smart_size.first_run", false);

pref("browser.cache.memory.enable", true);

#ifdef DEVICE_256MB_SUPPORT
pref("browser.cache.memory.capacity", 512); //kilobytes
#else
pref("browser.cache.memory.capacity", 1024); // kilobytes
#endif
pref("browser.cache.memory_limit", 2048); // 2 MB

/* image cache prefs */
#ifdef DEVICE_256MB_SUPPORT
pref("image.cache.size", 524288); //bytes
#else
pref("image.cache.size", 1048576); // bytes
#endif
pref("canvas.image.cache.limit", 20971520); // 20 MB
pref("canvas.mozgetasfile.enabled", true);

/* protocol warning prefs */
pref("network.protocol-handler.warn-external.tel", false);
pref("network.protocol-handler.warn-external.mailto", false);
pref("network.protocol-handler.warn-external.vnd.youtube", false);

/* http prefs */
pref("network.http.pipelining", true);
pref("network.http.pipelining.ssl", true);
pref("network.http.proxy.pipelining", true);
pref("network.http.pipelining.maxrequests" , 6);
pref("network.http.keep-alive.timeout", 109);
pref("network.http.max-connections", 20);
pref("network.http.max-persistent-connections-per-server", 6);
pref("network.http.max-persistent-connections-per-proxy", 20);

// Keep the old default of accepting all cookies,
// no matter if you already visited the website or not
pref("network.cookie.cookieBehavior", 0);

// spdy
pref("network.http.spdy.push-allowance", 32768);

pref("network.http.customheader.hosts", "ssp.kaiads.com,kaios-analytics.com,kaios-wallet.com");
pref("network.http.customheader.name", "X-Kai-Ads");
pref("network.http.customheader.version", "v1");

// See bug 545869 for details on why these are set the way they are
pref("network.buffer.cache.count", 24);
pref("network.buffer.cache.size",  16384);

// predictive actions
pref("network.predictor.enabled", false); // disabled on b2g
pref("network.predictor.max-db-size", 2097152); // bytes
pref("network.predictor.preserve", 50); // percentage of predictor data to keep when cleaning up

// Disable IPC security to let WebRTC works on https://appr.tc
pref("network.disable.ipc.security", true);

// Extend keepalive long lived idle time for push adaptive ping mechanism
// See KaiOS Bug 108187
pref("network.http.tcp_keepalive.long_lived_idle_time", 1800);

/* session history */
pref("browser.sessionhistory.max_entries", 50);
pref("browser.sessionhistory.contentViewerTimeout", 360);

/* session store */
pref("browser.sessionstore.resume_session_once", false);
pref("browser.sessionstore.resume_from_crash", true);
pref("browser.sessionstore.resume_from_crash_timeout", 60); // minutes
pref("browser.sessionstore.interval", 10000); // milliseconds
pref("browser.sessionstore.max_tabs_undo", 1);

/* these should help performance */
pref("mozilla.widget.force-24bpp", true);
pref("mozilla.widget.use-buffer-pixmap", true);
pref("mozilla.widget.disable-native-theme", true);
pref("layout.reflow.synthMouseMove", false);
#ifndef MOZ_X11
pref("layers.enable-tiles", true);
#endif
pref("layers.low-precision-buffer", true);
pref("layers.low-precision-opacity", "0.5");
pref("layers.progressive-paint", true);

/* download manager (don't show the window or alert) */
pref("browser.download.useDownloadDir", true);
pref("browser.download.folderList", 1); // Default to ~/Downloads
pref("browser.download.manager.showAlertOnComplete", false);
pref("browser.download.manager.showAlertInterval", 2000);
pref("browser.download.manager.retention", 2);
pref("browser.download.manager.showWhenStarting", false);
pref("browser.download.manager.closeWhenDone", true);
pref("browser.download.manager.openDelay", 0);
pref("browser.download.manager.focusWhenStarting", false);
pref("browser.download.manager.flashCount", 2);
pref("browser.download.manager.displayedHistoryDays", 7);

/* download helper */
pref("browser.helperApps.deleteTempFileOnExit", false);

/* password manager */
pref("signon.rememberSignons", true);
pref("signon.expireMasterPassword", false);

/* autocomplete */
pref("browser.formfill.enable", false);

/* spellcheck */
pref("layout.spellcheckDefault", 0);

/* block popups by default, and notify the user about blocked popups */
pref("dom.disable_open_during_load", true);
pref("privacy.popups.showBrowserMessage", true);

pref("keyword.enabled", true);
pref("browser.fixup.domainwhitelist.localhost", true);

pref("accessibility.typeaheadfind", false);
pref("accessibility.typeaheadfind.timeout", 5000);
pref("accessibility.typeaheadfind.flashBar", 1);
pref("accessibility.typeaheadfind.linksonly", false);
pref("accessibility.typeaheadfind.casesensitive", 0);

// SSL error page behaviour
pref("browser.ssl_override_behavior", 2);
pref("browser.xul.error_pages.expert_bad_cert", false);

// disable updating
pref("browser.search.update", false);

// tell the search service that we don't really expose the "current engine"
pref("browser.search.noCurrentEngine", true);

// enable xul error pages
pref("browser.xul.error_pages.enabled", true);

// disable color management
pref("gfx.color_management.mode", 0);

// don't allow JS to move and resize existing windows
pref("dom.disable_window_move_resize", true);

// prevent click image resizing for nsImageDocument
pref("browser.enable_click_image_resizing", false);

// controls which bits of private data to clear. by default we clear them all.
pref("privacy.item.cache", true);
pref("privacy.item.cookies", true);
pref("privacy.item.offlineApps", true);
pref("privacy.item.history", true);
pref("privacy.item.formdata", true);
pref("privacy.item.downloads", true);
pref("privacy.item.passwords", true);
pref("privacy.item.sessions", true);
pref("privacy.item.geolocation", true);
pref("privacy.item.siteSettings", true);
pref("privacy.item.syncAccount", true);

// base url for the wifi geolocation network provider
pref("geo.provider.use_mls", false);
pref("geo.cell.scan", true);

// URL for geolocating service, the original URL of B2G OS is
// "https://location.services.mozilla.com/v1/geolocate?key=%MOZILLA_API_KEY%"
// Empty string would disable WiFi/cell geolocating service
pref("geo.provider.network.url", "https://global.skyhookwireless.com/wps2/json/location?key=%GONK_GEOLOCATION_API_KEY%");

// whether to ask location provider to use IP address to determine location
pref("geo.provider.network.considerIp", false);

// the secret API key of location service
pref("geo.authorization.key", "%GONK_GEOLOCATION_API_KEY%");

// Whether to clean up location provider when Geolocation setting is turned off.
pref("geo.provider.ondemand_cleanup", true);

// enable geo
pref("geo.enabled", true);

#ifdef HAS_KOOST_MODULES
// enable GNSS monitor
pref("geo.gnssMonitor.enabled", true);
#endif

// content sink control -- controls responsiveness during page load
// see https://bugzilla.mozilla.org/show_bug.cgi?id=481566#c9
pref("content.sink.enable_perf_mode",  2); // 0 - switch, 1 - interactive, 2 - perf
pref("content.sink.pending_event_mode", 0);
pref("content.sink.perf_deflect_count", 1000000);
pref("content.sink.perf_parse_time", 50000000);

// Maximum scripts runtime before showing an alert
// Disable the watchdog thread for B2G. See bug 870043 comment 31.
pref("dom.use_watchdog", false);

// The slow script dialog can be triggered from inside the JS engine as well,
// ensure that those calls don't accidentally trigger the dialog.
pref("dom.max_script_run_time", 0);
pref("dom.max_chrome_script_run_time", 0);

// plugins
pref("plugin.disable", true);
pref("dom.ipc.plugins.enabled", true);

// product URLs
// The breakpad report server to link to in about:crashes
pref("breakpad.reportURL", "https://crash-stats.mozilla.com/report/index/");
pref("app.releaseNotesURL", "http://www.mozilla.com/%LOCALE%/b2g/%VERSION%/releasenotes/");
pref("app.support.baseURL", "http://support.mozilla.com/b2g");
pref("app.privacyURL", "http://www.mozilla.com/%LOCALE%/m/privacy.html");
pref("app.creditsURL", "http://www.mozilla.org/credits/");
pref("app.featuresURL", "http://www.mozilla.com/%LOCALE%/b2g/features/");
pref("app.faqURL", "http://www.mozilla.com/%LOCALE%/b2g/faq/");

// Name of alternate about: page for certificate errors (when undefined, defaults to about:neterror)
pref("security.alternate_certificate_error_page", "certerror");

pref("security.warn_viewing_mixed", false); // Warning is disabled.  See Bug 616712.

// Block insecure active content on https pages
pref("security.mixed_content.block_active_content", true);

// 2 = strict certificate pinning checks.
// This default preference is more strict than Firefox because B2G
// currently does not have a way to install local root certificates.
// Strict checking is effectively equivalent to non-strict checking as
// long as that is true.  If an ability to add local certificates is
// added, there may be a need to change this pref.
pref("security.cert_pinning.enforcement_level", 2);


// Override some named colors to avoid inverse OS themes
pref("ui.-moz-dialog", "#efebe7");
pref("ui.-moz-dialogtext", "#101010");
pref("ui.-moz-field", "#fff");
pref("ui.-moz-fieldtext", "#1a1a1a");
pref("ui.-moz-buttonhoverface", "#f3f0ed");
pref("ui.-moz-buttonhovertext", "#101010");
pref("ui.-moz-combobox", "#fff");
pref("ui.-moz-comboboxtext", "#101010");
pref("ui.buttonface", "#ece7e2");
pref("ui.buttonhighlight", "#fff");
pref("ui.buttonshadow", "#aea194");
pref("ui.buttontext", "#101010");
pref("ui.captiontext", "#101010");
pref("ui.graytext", "#b1a598");
pref("ui.highlighttext", "#1a1a1a");
pref("ui.threeddarkshadow", "#000");
pref("ui.threedface", "#ece7e2");
pref("ui.threedhighlight", "#fff");
pref("ui.threedlightshadow", "#ece7e2");
pref("ui.threedshadow", "#aea194");
pref("ui.windowframe", "#efebe7");
pref("ui.menu", "#f97c17");
pref("ui.menutext", "#ffffff");
pref("ui.infobackground", "#343e40");
pref("ui.infotext", "#686868");
pref("ui.window", "#ffffff");
pref("ui.windowtext", "#000000");
pref("ui.highlight", "#b2f2ff");

// replace newlines with spaces on paste into single-line text boxes
pref("editor.singleLine.pasteNewlines", 2);

// threshold where a tap becomes a drag, in 1/240" reference pixels
// The names of the preferences are to be in sync with EventStateManager.cpp
pref("ui.dragThresholdX", 25);
pref("ui.dragThresholdY", 25);

pref("dom.ipc.tabs.disabled", false);
pref("layers.acceleration.disabled", false);
pref("layers.async-pan-zoom.enabled", true);

// Web Notifications
pref("notification.feature.enabled", true);

// prevent video elements from preloading too much data
pref("media.preload.default", 1); // default to preload none
pref("media.preload.auto", 2);    // preload metadata if preload=auto
#ifdef DEVICE_256MB_SUPPORT
pref("media.cache_size", 2048);  //2MB media cache
#else
pref("media.cache_size", 4096);    // 4MB media cache
#endif
// Try to save battery by not resuming reading from a connection until we fall
// below 10s of buffered data.
pref("media.cache_resume_threshold", 10);
pref("media.cache_readahead_limit", 30);

#ifdef MOZ_FMP4
// Enable/Disable Gonk Decoder Module
pref("media.gonk.enabled", true);
#endif

#ifdef DEVICE_256MB_SUPPORT
// Set maximum video buffer size to 20MB(20*1024*1024)
pref("media.mediasource.eviction_threshold.video", 20971520); //byte
// Set maximum Audio buffer size to 10MB(10*1024*1024)
pref("media.mediasource.eviction_threshold.audio", 10485760); //byte
#else
// Set maximum Video buffer size to 40MB(40*1024*1024).
pref("media.mediasource.eviction_threshold.video", 41943040);
// Set maximum Audio buffer size to 20MB(20*1024*1024).
pref("media.mediasource.eviction_threshold.audio", 20971520);
#endif

// Encrypted media extensions.
pref("media.eme.enabled", true);
pref("media.eme.apiVisible", true);
// The default number of decoded video frames that are enqueued in
// MediaDecoderReader's mVideoQueue.
pref("media.video-queue.default-size", 3);

// videocontrols related settings.
pref("media.videocontrols.keyboard-tab-to-all-controls", false);
pref("media.videocontrols.keyboard-enter-to-toggle-pause", true);
pref("media.videocontrols.volume-control-override", true);

// Optimize images' memory usage
pref("image.downscale-during-decode.enabled", true);
pref("image.mem.allow_locking_in_content_processes", true);
// Limit the surface cache to 1/8 of main memory or 128MB, whichever is smaller.
// Almost everything that was factored into 'max_decoded_image_kb' is now stored
// in the surface cache.  1/8 of main memory is 32MB on a 256MB device, which is
// about the same as the old 'max_decoded_image_kb'.
#ifdef DEVICE_256MB_SUPPORT
pref("image.mem.surfacecache.max_size_kb", 8192);  // 8MB
pref("image.mem.surfacecache.size_factor", 32);  // 1/32 of main memory
#else
pref("image.mem.surfacecache.max_size_kb", 131072);  // 128MB
pref("image.mem.surfacecache.size_factor", 8);  // 1/8 of main memory
#endif
pref("image.mem.surfacecache.discard_factor", 2);  // Discard 1/2 of the surface cache at a time.
pref("image.mem.surfacecache.min_expiration_ms", 86400000); // 24h, we rely on the out of memory hook

pref("dom.w3c_touch_events.safetyX", 0); // escape borders in units of 1/240"
pref("dom.w3c_touch_events.safetyY", 120); // escape borders in units of 1/240"

#ifdef MOZ_SAFE_BROWSING
pref("browser.safebrowsing.enabled", true);
pref("browser.contentblocking.features.standard", "-tp,tpPrivate,cookieBehavior0,-cm,-fp");
pref("browser.contentblocking.features.strict", "tp,tpPrivate,cookieBehavior5,cookieBehaviorPBM5,cm,fp,stp,lvl2");
pref("browser.contentblocking.features.disable", "-tp,-tpPrivate,cookieBehavior0,-cm,-fp");
// Prevent loading of pages identified as malware
pref("browser.safebrowsing.malware.enabled", true);
pref("browser.safebrowsing.downloads.enabled", true);
pref("browser.safebrowsing.downloads.remote.enabled", true);
pref("browser.safebrowsing.downloads.remote.timeout_ms", 10000);
pref("browser.safebrowsing.downloads.remote.url", "https://sb-ssl.google.com/safebrowsing/clientreport/download?key=%GOOGLE_API_KEY%");
pref("browser.safebrowsing.downloads.remote.block_dangerous",            true);
pref("browser.safebrowsing.downloads.remote.block_dangerous_host",       true);
pref("browser.safebrowsing.downloads.remote.block_potentially_unwanted", false);
pref("browser.safebrowsing.downloads.remote.block_uncommon",             false);
pref("browser.safebrowsing.debug", false);

pref("browser.safebrowsing.provider.google.lists", "goog-badbinurl-shavar,goog-downloadwhite-digest256,goog-phish-shavar,goog-malware-shavar,goog-unwanted-shavar");
pref("browser.safebrowsing.provider.google.updateURL", "https://safebrowsing.google.com/safebrowsing/downloads?client=SAFEBROWSING_ID&appver=%VERSION%&pver=2.2&key=%GOOGLE_API_KEY%");
pref("browser.safebrowsing.provider.google.gethashURL", "https://safebrowsing.google.com/safebrowsing/gethash?client=SAFEBROWSING_ID&appver=%VERSION%&pver=2.2");
pref("browser.safebrowsing.provider.google.reportURL", "https://safebrowsing.google.com/safebrowsing/diagnostic?client=%NAME%&hl=%LOCALE%&site=");

pref("browser.safebrowsing.reportPhishMistakeURL", "https://%LOCALE%.phish-error.mozilla.com/?hl=%LOCALE%&url=");
pref("browser.safebrowsing.reportPhishURL", "https://%LOCALE%.phish-report.mozilla.com/?hl=%LOCALE%&url=");
pref("browser.safebrowsing.reportMalwareMistakeURL", "https://%LOCALE%.malware-error.mozilla.com/?hl=%LOCALE%&url=");

pref("browser.safebrowsing.id", "Firefox");

// Tables for application reputation.
pref("urlclassifier.downloadBlockTable", "goog-badbinurl-shavar");

// The number of random entries to send with a gethash request.
pref("urlclassifier.gethashnoise", 4);

// Gethash timeout for Safebrowsing.
pref("urlclassifier.gethash.timeout_ms", 5000);

// If an urlclassifier table has not been updated in this number of seconds,
// a gethash request will be forced to check that the result is still in
// the database.
pref("urlclassifier.max-complete-age", 2700);

// Tracking protection
pref("privacy.trackingprotection.lower_network_priority", true);
#endif

// True if this is the first time we are showing about:firstrun
pref("browser.firstrun.show.uidiscovery", true);
pref("browser.firstrun.show.localepicker", true);

// initiated by a user
pref("content.ime.strict_policy", true);

// True if you always want dump() to work
//
// On Android, you also need to do the following for the output
// to show up in logcat:
//
// $ adb shell stop
// $ adb shell setprop log.redirect-stdio true
// $ adb shell start
pref("browser.dom.window.dump.enabled", false);

// handle links targeting new windows
// 1=current window/tab, 2=new window, 3=new tab in most recent window
pref("browser.link.open_newwindow", 3);

// 0: no restrictions - divert everything
// 1: don't divert window.open at all
// 2: don't divert window.open with features
pref("browser.link.open_newwindow.restriction", 0);

// Enable a (virtually) unlimited number of content processes.
// We'll run out of PIDs on UNIX-y systems before we hit this limit.
pref("dom.ipc.processCount", 100000);

pref("dom.ipc.browser_frames.oop_by_default", false);

pref("dom.meta-viewport.enabled", true);

// Enable the Visual Viewport API
pref("dom.visualviewport.enabled", true);

// SMS/MMS
pref("dom.sms.enabled", true);

//The waiting time in network manager.
pref("network.gonk.ms-release-mms-connection", 30000);

// Shortnumber matching needed for e.g. Brazil:
// 03187654321 can be found with 87654321
pref("dom.phonenumber.substringmatching.BR", 8);
pref("dom.phonenumber.substringmatching.CO", 10);
pref("dom.phonenumber.substringmatching.VE", 7);
pref("dom.phonenumber.substringmatching.CL", 8);
pref("dom.phonenumber.substringmatching.PE", 7);

// WebAlarms
pref("dom.alarm.enabled", true);

// NetworkStats
#ifdef MOZ_WIDGET_GONK
pref("dom.networkStats.enabled", true);
#endif

// ResourceStats
#ifdef MOZ_WIDGET_GONK
pref("dom.resource_stats.enabled", true);
#endif

pref("dom.permissions.manager.enabled", true);

// controls if we want camera support
pref("device.camera.enabled", true);
// Empirically, this is the value returned by hal::GetTotalSystemMemory()
// when Flame's memory is limited to 512MiB. If the camera stack determines
// it is running on a low memory platform, features that can be reliably
// supported will be disabled. This threshold can be adjusted to suit other
// platforms; and set to 0 to disable the low-memory check altogether.
pref("camera.control.low_memory_thresholdMB", 404);
pref("media.realtime_decoder.enabled", true);

// TCPSocket
pref("dom.mozTCPSocket.enabled", true);

pref("full-screen-api.enabled", true);

#ifndef MOZ_WIDGET_GONK
// If we're not actually on physical hardware, don't make the top level widget
// fullscreen when transitioning to fullscreen. This means in emulated
// environments (like the b2g desktop client) we won't make the client window
// fill the whole screen, we'll just make the content fill the client window,
// i.e. it won't give the impression to content that the number of device
// screen pixels changes!
pref("full-screen-api.ignore-widgets", true);
#endif

pref("media.volume.steps", 10);

#ifdef MOZ_UPDATER
// When we're applying updates, we can't let anything hang us on
// quit+restart.  The user has no recourse.
pref("shutdown.watchdog.timeoutSecs", 10);
// Timeout before the update prompt automatically installs the update
pref("b2g.update.apply-prompt-timeout", 60000); // milliseconds
// Amount of time to wait after the user is idle before prompting to apply an update
pref("b2g.update.apply-idle-timeout", 600000); // milliseconds
// Amount of time after which connection will be restarted if no progress
pref("b2g.update.download-watchdog-timeout", 120000); // milliseconds
pref("b2g.update.download-watchdog-max-retries", 5);

pref("app.update.enabled", true);
pref("app.update.auto", false);
pref("app.update.silent", false);
pref("app.update.mode", 0);
pref("app.update.incompatible.mode", 0);
pref("app.update.staging.enabled", true);
pref("app.update.service.enabled", true);

pref("app.update.url", "https://aus5.mozilla.org/update/5/%PRODUCT%/%VERSION%/%BUILD_ID%/%PRODUCT_DEVICE%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/%IMEI%/update.xml");
pref("app.update.channel", "@MOZ_UPDATE_CHANNEL@");

// Interval at which update manifest is fetched.  In units of seconds.
pref("app.update.interval", 86400); // 1 day
// Don't throttle background updates.
pref("app.update.download.backgroundInterval", 0);

// Retry update socket connections every 30 seconds in the cases of certain kinds of errors
pref("app.update.socket.retryTimeout", 30000);

// Max of 20 consecutive retries (total 10 minutes) before giving up and marking
// the update download as failed.
// Note: Offline errors will always retry when the network comes online.
pref("app.update.socket.maxErrors", 20);

// Enable update logging for now, to diagnose growing pains in the
// field.
pref("app.update.log", true);
#else
// Explicitly disable the shutdown watchdog.  It's enabled by default.
// When the updater is disabled, we want to know about shutdown hangs.
pref("shutdown.watchdog.timeoutSecs", -1);
#endif

// Extensions preferences
pref("extensions.update.enabled", false);
pref("extensions.getAddons.cache.enabled", false);

// Context Menu
pref("ui.click_hold_context_menus", true);
pref("ui.click_hold_context_menus.delay", 400);

// Enable device storage
pref("device.storage.enabled", true);

pref("media.plugins.enabled", false);
pref("media.omx.enabled", true);

// Disable printing (particularly, window.print())
pref("dom.disable_window_print", true);

// Disable window.showModalDialog
pref("dom.disable_window_showModalDialog", true);

// Enable new experimental html forms
pref("dom.experimental_forms", true);
pref("dom.forms.number", true);

// Don't enable <input type=color> yet as we don't have a color picker
// implemented for b2g (bug 875751)
pref("dom.forms.color", false);

// This preference instructs the JS engine to discard the
// source of any privileged JS after compilation. This saves
// memory, but makes things like Function.prototype.toSource()
// fail.
pref("javascript.options.discardSystemSource", true);

// nsJSEnvironmentObserver observes the memory-pressure notifications and
// forces a garbage collection and cycle collection when it happens.
// The preference is false on ANDROID macro, because android already handles
// `memory-pressure` elsewhere.
// This overrides the value in StaticPrefs, in the circumstance that gonk defines
// ANDROID, but should set the value different from android.
pref("javascript.options.gc_on_memory_pressure", true);

// XXXX REMOVE FOR PRODUCTION. Turns on GC and CC logging
pref("javascript.options.mem.log", false);

// Increase mark slice time from 10ms to 30ms
pref("javascript.options.mem.gc_incremental_slice_ms", 30);

// Increase time to get more high frequency GC on benchmarks from 1000ms to 1500ms
pref("javascript.options.mem.gc_high_frequency_time_limit_ms", 1500);

pref("javascript.options.mem.gc_high_frequency_heap_growth_max", 300);
pref("javascript.options.mem.gc_high_frequency_heap_growth_min", 120);
pref("javascript.options.mem.gc_high_frequency_high_limit_mb", 40);
pref("javascript.options.mem.gc_high_frequency_low_limit_mb", 0);
pref("javascript.options.mem.gc_low_frequency_heap_growth", 120);
pref("javascript.options.mem.high_water_mark", 6);
pref("javascript.options.mem.gc_allocation_threshold_mb", 1);
pref("javascript.options.mem.gc_decommit_threshold_mb", 1);
pref("javascript.options.mem.gc_min_empty_chunk_count", 1);
pref("javascript.options.mem.gc_max_empty_chunk_count", 2);

// Show/Hide scrollbars when active/inactive
pref("ui.showHideScrollbars", 1);
pref("ui.useOverlayScrollbars", 1);
pref("ui.scrollbarFadeBeginDelay", 1800);
pref("ui.scrollbarFadeDuration", 0);

// Scrollbar position follows the document `dir` attribute
pref("layout.scrollbar.side", 1);

// CSS Scroll Snapping
pref("layout.css.scroll-snap.enabled", true);

// CSS: @media (prefers-contrast)
// true: prefers-contrast will toggle based on OS and browser settings.
// false: prefers-contrast will only parse and toggle in the chrome and ua.
pref("layout.css.prefers-contrast.enabled", true);

// Enable the ProcessPriorityManager, and give processes with no visible
// documents a 1s grace period before they're eligible to be marked as
// background. Background processes that are perceivable due to playing
// media are given a longer grace period to accomodate changing tracks, etc.
pref("dom.ipc.processPriorityManager.enabled", true);
pref("dom.ipc.processPriorityManager.backgroundGracePeriodMS", 1000);
pref("dom.ipc.processPriorityManager.backgroundPerceivableGracePeriodMS", 5000);
pref("dom.ipc.processPriorityManager.temporaryPriorityLockMS", 5000);

// Number of different background/foreground levels for background/foreground
// processes.  We use these different levels to force the low-memory killer to
// kill processes in a LRU order.
pref("dom.ipc.processPriorityManager.BACKGROUND.LRUPoolLevels", 5);
pref("dom.ipc.processPriorityManager.BACKGROUND_PERCEIVABLE.LRUPoolLevels", 4);

// Kernel parameters for process priorities.  These affect how processes are
// killed on low-memory and their relative CPU priorities.
//
// The kernel can only accept 6 (OomScoreAdjust, KillUnderKB) pairs. But it is
// okay, kernel will still kill processes with larger OomScoreAdjust first even
// its OomScoreAdjust don't have a corresponding KillUnderKB.

pref("hal.processPriorityManager.gonk.MASTER.OomScoreAdjust", 0);
pref("hal.processPriorityManager.gonk.MASTER.KillUnderKB", 4096);
pref("hal.processPriorityManager.gonk.MASTER.cgroup", "");

pref("hal.processPriorityManager.gonk.PREALLOC.OomScoreAdjust", 67);
pref("hal.processPriorityManager.gonk.PREALLOC.cgroup", "apps/bg_non_interactive");

pref("hal.processPriorityManager.gonk.FOREGROUND_HIGH.OomScoreAdjust", 67);
pref("hal.processPriorityManager.gonk.FOREGROUND_HIGH.KillUnderKB", 5120);
pref("hal.processPriorityManager.gonk.FOREGROUND_HIGH.cgroup", "apps/critical");

pref("hal.processPriorityManager.gonk.FOREGROUND.OomScoreAdjust", 134);
#ifdef DEVICE_256MB_SUPPORT
pref("hal.processPriorityManager.gonk.FOREGROUND.KillUnderKB", 6144);
#else
pref("hal.processPriorityManager.gonk.FOREGROUND.KillUnderKB", 20480);
#endif
pref("hal.processPriorityManager.gonk.FOREGROUND.cgroup", "apps");

pref("hal.processPriorityManager.gonk.FOREGROUND_KEYBOARD.OomScoreAdjust", 200);
pref("hal.processPriorityManager.gonk.FOREGROUND_KEYBOARD.cgroup", "apps");

pref("hal.processPriorityManager.gonk.BACKGROUND_PERCEIVABLE.OomScoreAdjust", 400);
#ifdef DEVICE_256MB_SUPPORT
pref("hal.processPriorityManager.gonk.BACKGROUND_PERCEIVABLE.KillUnderKB", 8192);
#else
pref("hal.processPriorityManager.gonk.BACKGROUND_PERCEIVABLE.KillUnderKB", 40960);
#endif
pref("hal.processPriorityManager.gonk.BACKGROUND_PERCEIVABLE.cgroup", "apps/bg_perceivable");

pref("hal.processPriorityManager.gonk.BACKGROUND.OomScoreAdjust", 667);
#ifdef DEVICE_256MB_SUPPORT
pref("hal.processPriorityManager.gonk.BACKGROUND.KillUnderKB", 20480);
#else
pref("hal.processPriorityManager.gonk.BACKGROUND.KillUnderKB", 65536);
#endif
pref("hal.processPriorityManager.gonk.BACKGROUND.cgroup", "apps/bg_non_interactive");

// Control group definitions (i.e., CPU priority groups) for B2G processes.
//
// memory_swappiness -   0 - The kernel will swap only to avoid an out of memory condition
// memory_swappiness -  60 - The default value.
// memory_swappiness - 100 - The kernel will swap aggressively.

// Foreground apps
pref("hal.processPriorityManager.gonk.cgroups.apps.cpu_shares", 1024);
pref("hal.processPriorityManager.gonk.cgroups.apps.cpu_notify_on_migrate", 0);
pref("hal.processPriorityManager.gonk.cgroups.apps.memory_swappiness", 10);

// Foreground apps with high priority, 16x more CPU than foreground ones
pref("hal.processPriorityManager.gonk.cgroups.apps/critical.cpu_shares", 16384);
pref("hal.processPriorityManager.gonk.cgroups.apps/critical.cpu_notify_on_migrate", 0);
pref("hal.processPriorityManager.gonk.cgroups.apps/critical.memory_swappiness", 0);

// Background perceivable apps, ~10x less CPU than foreground ones
pref("hal.processPriorityManager.gonk.cgroups.apps/bg_perceivable.cpu_shares", 103);
pref("hal.processPriorityManager.gonk.cgroups.apps/bg_perceivable.cpu_notify_on_migrate", 0);
pref("hal.processPriorityManager.gonk.cgroups.apps/bg_perceivable.memory_swappiness", 60);

// Background apps, ~20x less CPU than foreground ones and ~2x less than perceivable ones
pref("hal.processPriorityManager.gonk.cgroups.apps/bg_non_interactive.cpu_shares", 52);
pref("hal.processPriorityManager.gonk.cgroups.apps/bg_non_interactive.cpu_notify_on_migrate", 0);
pref("hal.processPriorityManager.gonk.cgroups.apps/bg_non_interactive.memory_swappiness", 100);

// By default the compositor thread on gonk runs without real-time priority.  RT
// priority can be enabled by setting this pref to a value between 1 and 99.
// Note that audio processing currently runs at RT priority 2 or 3 at most.
//
// If RT priority is disabled, then the compositor nice value is used. We prefer
// to use a nice value of -4, which matches Android's preferences. Setting a preference
// of RT priority 1 would mean it is higher than audio, which is -16. The compositor
// priority must be below the audio thread.
//
// Do not change these values without gfx team review.
pref("hal.gonk.COMPOSITOR.rt_priority", 0);
pref("hal.gonk.COMPOSITOR.nice", -4);

// Fire a memory pressure event when the system has less than Xmb of memory
// remaining.  You should probably set this just above Y.KillUnderKB for
// the highest priority class Y that you want to make an effort to keep alive.
// (For example, we want BACKGROUND_PERCEIVABLE to stay alive.)  If you set
// this too high, then we'll send out a memory pressure event every Z seconds
// (see below), even while we have processes that we would happily kill in
// order to free up memory.
#ifdef DEVICE_256MB_SUPPORT
pref("gonk.notifyHardLowMemUnderKB", 14336); //kilobytes
#else
pref("gonk.notifyHardLowMemUnderKB", 30720);
#endif

// Fire a memory pressure event when the system has less than Xmb of memory
// remaining and then switch to the hard trigger, see above.  This should be
// placed above the BACKGROUND priority class.
#ifdef DEVICE_256MB_SUPPORT
pref("gonk.notifySoftLowMemUnderKB", 14336); //kilobytes
#else
pref("gonk.notifySoftLowMemUnderKB", 30720);
#endif

// We wait this long before polling the memory-pressure fd after seeing one
// memory pressure event.  (When we're not under memory pressure, we sit
// blocked on a poll(), and this pref has no effect.)
pref("gonk.systemMemoryPressureRecoveryPollMS", 5000);

// Ignore the "dialog=1" feature in window.open.
pref("dom.disable_window_open_dialog_feature", true);

// Screen reader support
pref("accessibility.accessfu.activate", 2);
pref("accessibility.accessfu.quicknav_modes", "Link,Heading,FormElement,Landmark,ListItem");
// Active quicknav mode, index value of list from quicknav_modes
pref("accessibility.accessfu.quicknav_index", 0);
// Setting for an utterance order (0 - description first, 1 - description last).
pref("accessibility.accessfu.utterance", 1);
// Whether to skip images with empty alt text
pref("accessibility.accessfu.skip_empty_images", true);
// Setting to change the verbosity of entered text (0 - none, 1 - characters,
// 2 - words, 3 - both)
pref("accessibility.accessfu.keyboard_echo", 3);

// Enable hit-target fluffing
pref("ui.touch.radius.enabled", true);
pref("ui.touch.radius.leftmm", 3);
pref("ui.touch.radius.topmm", 5);
pref("ui.touch.radius.rightmm", 3);
pref("ui.touch.radius.bottommm", 2);

pref("ui.mouse.radius.enabled", true);
pref("ui.mouse.radius.leftmm", 3);
pref("ui.mouse.radius.topmm", 5);
pref("ui.mouse.radius.rightmm", 3);
pref("ui.mouse.radius.bottommm", 2);

// Disable native prompt
pref("browser.prompt.allowNative", false);

// Minimum delay in milliseconds between network activity notifications (0 means
// no notifications). The delay is the same for both download and upload, though
// they are handled separately. This pref is only read once at startup:
// a restart is required to enable a new value.
pref("network.activity.blipIntervalMilliseconds", 250);

// By default we want the NetworkManager service to manage Gecko's offline
// status for us according to the state of Wifi/cellular data connections.
// In some environments, such as the emulator or hardware with other network
// connectivity, this is not desireable, however, in which case this pref
// can be flipped to false.
pref("network.gonk.manage-offline-status", true);

// Enable font inflation for browser tab content.
pref("font.size.inflation.minTwips", 120);
// And disable it for lingering master-process UI.
pref("font.size.inflation.disabledInMasterProcess", true);

// Enable freeing dirty pages when minimizing memory; this reduces memory
// consumption when applications are sent to the background.
pref("memory.free_dirty_pages", true);

// Enable the Linux-specific, system-wide memory reporter.
pref("memory.system_memory_reporter", true);

// Don't dump memory reports on OOM, by default.
pref("memory.dump_reports_on_oom", false);

pref("layout.framevisibility.numscrollportwidths", 1);
pref("layout.framevisibility.numscrollportheights", 1);

// Wait up to this much milliseconds when orientation changed
pref("layers.orientation.sync.timeout", 1000);

// Animate the orientation change
pref("b2g.orientation.animate", true);

// Don't discard WebGL contexts for foreground apps on memory
// pressure.
pref("webgl.can-lose-context-in-foreground", false);

// Allow nsMemoryInfoDumper to create a fifo in the temp directory.  We use
// this fifo to trigger about:memory dumps, among other things.
pref("memory_info_dumper.watch_fifo.enabled", true);
pref("memory_info_dumper.watch_fifo.directory", "/data/local");

// See ua-update.json.in for the packaged UA override list
pref("general.useragent.updates.enabled", false);
pref("general.useragent.updates.url", "https://dynamicua.cdn.mozilla.net/0/%APP_ID%");
pref("general.useragent.updates.interval", 604800); // 1 week
pref("general.useragent.updates.retry", 86400); // 1 day
// Device ID can be composed of letter, numbers, hyphen ("-") and dot (".")
pref("general.useragent.device_id", "");

// Add Mozilla AudioChannel APIs.
pref("media.useAudioChannelAPI", true);

pref("b2g.version", "@MOZ_B2G_VERSION@");
pref("b2g.osName", "@MOZ_B2G_OS_NAME@");

// Disable console buffering to save memory.
pref("consoleservice.buffered", false);

#ifdef MOZ_WIDGET_GONK
// Performance testing suggests 2k is a better page size for SQLite.
pref("toolkit.storage.pageSize", 2048);
#endif

// Enable the disk space watcher
pref("disk_space_watcher.enabled", true);

// SNTP preferences.
pref("network.sntp.maxRetryCount", 10);
pref("network.sntp.refreshPeriod", 86400); // In seconds.
pref("network.sntp.pools", // Servers separated by ';'.
     "0.pool.ntp.org;1.pool.ntp.org;2.pool.ntp.org;3.pool.ntp.org");
pref("network.sntp.port", 123);
pref("network.sntp.timeout", 30); // In seconds.

// Allow ADB to run for this many hours before disabling
// (only applies when marionette is disabled)
// 0 disables the timer.
pref("b2g.adb.timeout-hours", 12);

// Absolute path to the devtool unix domain socket file used
// to communicate with a usb cable via adb forward
// Firefox's devtools expect a socket path containing "firefox-debugger-socket", see
// https://hg.mozilla.org/mozilla-central/file/99f1b315fa89db68ef3e7f8875bbd0a4b818423d/devtools/shared/adb/adb-device.js#l36
#ifdef MOZ_WIDGET_GONK
pref("devtools.debugger.unix-domain-socket", "/data/local/firefox-debugger-socket");
#else
pref("devtools.debugger.unix-domain-socket", "");
pref("devtools.debugger.remote-port", 6222);
#endif
pref("devtools.remote.usb.enabled", true);
pref("devtools.remote.wifi.enabled", false);
pref("devtools.inspector.inactive.css.enabled", false);

// enable Skia/GL (OpenGL-accelerated 2D drawing) for large enough 2d canvases,
// falling back to Skia/software for smaller canvases
#ifdef MOZ_WIDGET_GONK
pref("gfx.canvas.azure.backends", "skia");
pref("gfx.canvas.azure.accelerated", true);
#endif

// Turn on dynamic cache size for Skia
pref("gfx.canvas.skiagl.dynamic-cache", true);

// Limit skia to canvases the size of the device screen or smaller
pref("gfx.canvas.max-size-for-skia-gl", -1);

// enable fence with readpixels for SurfaceStream
pref("gfx.gralloc.fence-with-readpixels", true);

// enable screen mirroring to external display
pref("gfx.screen-mirroring.enabled", false);

// The url of the page used to display network error details.
pref("b2g.neterror.url", "net_error.html");

// Enable PAC generator for B2G.
pref("network.proxy.pac_generator", true);

// Enable Web Speech synthesis API
pref("media.webspeech.synth.enabled", true);

// Enable Web Speech recognition API
pref("media.webspeech.recognition.enable", true);

// Downloads API
pref("dom.downloads.enabled", true);

// External Helper Application Handling
//
// All external helper application handling can require the docshell to be
// active before allowing the external helper app service to handle content.
//
// To prevent SD card DoS attacks via downloads we disable background handling.
//
pref("security.exthelperapp.disable_background_handling", true);

// Inactivity time in milliseconds after which we shut down the OS.File worker.
pref("osfile.reset_worker_delay", 5000);

// APZ physics settings, tuned by UX designers
pref("apz.axis_lock.mode", 2); // Use "sticky" axis locking
pref("apz.fling_curve_function_x1", "0.41");
pref("apz.fling_curve_function_y1", "0.0");
pref("apz.fling_curve_function_x2", "0.80");
pref("apz.fling_curve_function_y2", "1.0");
pref("apz.fling_curve_threshold_inches_per_ms", "0.01");
pref("apz.fling_friction", "0.0019");
pref("apz.max_velocity_inches_per_ms", "0.07");
pref("apz.overscroll.enabled", true);
pref("apz.displayport_expiry_ms", 0); // causes issues on B2G, see bug 1250924

// For event-regions based hit-testing
pref("layout.event-regions.enabled", true);

// This preference allows FirefoxOS apps (and content, I think) to force
// the use of software (instead of hardware accelerated) 2D canvases by
// creating a context like this:
//
//   canvas.getContext('2d', { willReadFrequently: true })
//
// Using a software canvas can save memory when JS calls getImageData()
// on the canvas frequently. See bug 884226.
pref("gfx.canvas.willReadFrequently.enable", true);

// Disable autofocus until we can have it not bring up the keyboard.
// https://bugzilla.mozilla.org/show_bug.cgi?id=965763
pref("browser.autofocus", false);

// Enable wakelock
pref("dom.wakelock.enabled", true);

// Overwrite the size in all.js
pref("layout.accessiblecaret.width", "14");
pref("layout.accessiblecaret.height", "14");
pref("layout.accessiblecaret.margin-left", "0");
pref("layout.accessiblecaret.caret_shown_when_long_tapping_on_empty_content", true);
pref("layout.accessiblecaret.always_tilt", true);
pref("layout.accessiblecaret.transition-duration", "0");
pref("layout.accessiblecaret.bar.enabled", true);
pref("layout.accessiblecaret.bar.cursor_enabled", true);

// Enable mapped array buffer.
#ifndef XP_WIN
pref("dom.mapped_arraybuffer.enabled", true);
#endif

// UDPSocket API
pref("dom.udpsocket.enabled", true);

// Enable TV Manager API
pref("dom.tv.enabled", false);
// Enable TV Simulator API
pref("dom.tv.simulator.enabled", false);

// Enable Inputport Manager API
pref("dom.inputport.enabled", true);

// ServiceWorker API
pref("dom.serviceWorkers.enabled", true);
// Allow service workers to open windows for a longer period after a notification
// click on mobile.  This is to account for some devices being quite slow.
pref("dom.serviceWorkers.disable_open_click_delay", 5000);
// The amount of time (milliseconds) service workers can be kept running using waitUntil promises
// or executing "long-running" JS after the "idle_timeout" period has expired.
pref("dom.serviceWorkers.idle_extended_timeout", 300000);
pref("dom.serviceWorkers.shutdown_observer.enabled", true);

// Enable W3C Push API
pref("dom.webnotifications.serviceworker.enabled", true);
pref("dom.webnotifications.serviceworker.maxActions", 2);
pref("dom.push.enabled", true);
// extend request timeout if fetching token is required
pref("dom.push.extendTimeout.token", 3000);

// Adaptive ping
pref("dom.push.adaptive.lastGoodPingInterval", 180000);// 3 min
pref("dom.push.adaptive.lastGoodPingInterval.mobile", 180000);// 3 min
pref("dom.push.adaptive.lastGoodPingInterval.wifi", 180000);// 3 min
// Valid gap between the biggest good ping and the bad ping
pref("dom.push.adaptive.gap", 60000); // 1 minute
// We limit the ping to this maximum value
pref("dom.push.adaptive.upperLimit", 1740000); // 29 min
pref("dom.push.pingInterval.default", 180000);// 3 min
pref("dom.push.pingInterval.mobile", 180000); // 3 min
pref("dom.push.pingInterval.wifi", 180000);  // 3 min

// Retain at most 10 processes' layers buffers
pref("layers.compositor-lru-size", 10);

// Disable webxr and vr.
pref("dom.vr.webxr.enabled", false);
pref("dom.vr.enabled", false);

// Disable webdriver which will trigger insecure browser error.
pref("dom.webdriver.enabled", false);

// In B2G by deafult any AudioChannelAgent is muted when created.
pref("dom.audiochannel.mutedByDefault", true);

// The app origin of bluetooth app, which is responsible for listening pairing
// requests.
pref("dom.bluetooth.app-origin", "app://bluetooth.gaiamobile.org");

// Enable W3C WebBluetooth API and disable B2G only GATT client API.
pref("dom.bluetooth.webbluetooth.enabled", false);

// Enable flip manager
pref("dom.flip.enabled", true);

// Enable flashlight manager
pref("dom.flashlight.enabled", true);

// Default device name for Presentation API
pref("dom.presentation.device.name", "B2G OS");

// Enable notification of performance timing
pref("dom.performance.enable_notify_performance_timing", true);

// Multi-screen
pref("b2g.multiscreen.enabled", true);
pref("b2g.multiscreen.chrome_remote_url", "chrome://b2g/content/shell_remote.html");
pref("b2g.multiscreen.system_remote_url", "chrome://system/content/index_remote.html");
pref("toolkit.multiscreen.defaultChromeFeatures", "chrome,dialog=no,close,resizable,scrollbars,extrachrome");

// Blocklist service
pref("extensions.blocklist.enabled", true);
pref("extensions.blocklist.interval", 86400);
pref("extensions.blocklist.url", "https://blocklist.addons.mozilla.org/blocklist/3/%APP_ID%/%APP_VERSION%/%PRODUCT%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/%PING_COUNT%/%TOTAL_PING_COUNT%/%DAYS_SINCE_LAST_PING%/");
pref("extensions.blocklist.detailsURL", "https://www.mozilla.com/%LOCALE%/blocklist/");

// Google geoCoding URL.
// format as https://maps.googleapis.com/maps/api/geocode/json?latlng=<lat>,<lon>&key=<key>
pref("google.geocoding.URL", "https://maps.googleapis.com/maps/api/geocode/json?latlng=");

// Enable the GLCursor and DOM virtual cursor on non-touch devices.
// GLCursor draws a cursor image if widget receives eMouseMove event.
// DOM virtual cursor simulate mouse events by keyboard events.
pref("dom.virtualcursor.enabled", true);

// Enable the pan simulation of the virtual cursor service on non-touch
// devices.
pref("dom.virtualcursor.pan_simulator.enabled", true);

// A step offset in pixel when short click direction keys to move the cursor.
pref("dom.virtualcursor.move.short_click_offset", "10.0");

// Enable to simulate an microphonetoggle key event,
// only when device do not have hardware key.
pref("dom.microphonetoggle.supported", true);
// Device has a dedicated microphonetoggle key or not.
pref("dom.microphonetoggle.hardwareKey", false);

// Dispatch the function key events to the content first.
pref("dom.keyboardevent.dispatch_function_keys_to_content_first", true);
pref("dom.keyboardevent.function_keys", "Backspace,MicrophoneToggle,GoBack,EndCall,AudioVolumeDown,AudioVolumeUp");

// Enable keyboardEventGenerator on touch devices.
pref("dom.keyboardEventGenerator.enabled", false);

// In compatibility mode, 'Firefox' will be added to UA.
pref("general.useragent.compatMode.firefox", true);

// Disable Gecko Telemetry
pref("toolkit.telemetry.enabled", false);

// Set adaptor as default recognition service
pref("media.webspeech.service.default", "adaptor");

// This defines the performance interfaces, do not turn it off.
pref('dom.enable_user_timing', true);
#ifdef TARGET_VARIANT_ENG
// Turn on logging performance marker in engineering build.
pref('dom.performance.enable_user_timing_logging', true);
#else
// Turn off logging performance marker in user/userdebug build.
pref('dom.performance.enable_user_timing_logging', false);
#endif

// Turn off update of dummy thermal status
pref('dom.battery.test.dummy_thermal_status', false);

// Turn off update of dummy battery level
pref('dom.battery.test.dummy_battery_level', false);

// Disable hang monitor, see bug 24699
pref("hangmonitor.timeout", 0);

// level 0 means disable log
pref("hangmonitor.log.level", 1);

#ifdef B2G_RIL
pref("b2g.ril.enabled", true);
#else
pref("b2g.ril.enabled", false);
#endif

#ifdef DISABLE_WIFI
// Disable Gecko wifi
pref("device.capability.wifi", false);
#endif

pref("device.dfc", false);

pref("device.dual-lte", false);

// Support wifi passpoint
pref("dom.passpoint.supported", false);

// Control wifi during emergency session
pref("dom.emergency.wifi-control", true);

// Customize whether tethering could be turned on again after wifi is turned off
pref("wifi.affect.tethering", false);
// Customize whether wifi could be turned on again after tethering is turned off
pref("tethering.affect.wifi", false);

// Setup IPv6 address generate mode in Gecko
pref("dom.b2g_ipv6_addr_mode", 0);

// Support primary sim switch
pref("ril.support.primarysim.switch", false);

// Enable app cell broadcast list configuration (apn.json)
pref("dom.app_cb_configuration", true);

// Use chrome://b2g to use the in-tree system app, and chrome://system to
// load it from either /system/b2g/webapps or /data/local/webapps
// We keep the internal one has default to ensure we start.
pref("b2g.system_startup_url", "chrome://b2g/content/system/index.html");
// pref("b2g.system_startup_url", "chrome://system/content/index.html");

// Use system app browser url
#if !defined(API_DAEMON_PORT) || API_DAEMON_PORT == 80
pref("b2g.system_app_browser_url", "http://system.localhost/browser/browser.html");
#else
pref("b2g.system_app_browser_url", "http://system.localhost:@API_DAEMON_PORT@/browser/browser.html");
#endif

pref("devtools.console.stdout.content", true);

// Enable touch events and pointer events by default.
pref("dom.w3c_touch_events.enabled", 1);
pref("dom.w3c_pointer_events.enabled", true);

pref("extensions.systemAddon.update.enabled", false);

pref("browser.privatebrowsing.autostart", false);

pref("security.sandbox.content.level", 0);

pref("gfx.e10s.font-list.shared", true);

pref("dom.systemMessage.enabled", true);

#ifndef TARGET_VARIANT_USER
// Remote debugging is disabled by default in MOZ_OFFICIAL builds.
pref("devtools.debugger.remote-enabled", true);
// To get connection with remote debugger working in non-nightly builds.
pref("devtools.debugger.prompt-connection", false);
pref("devtools.console.stdout.chrome", true);
pref("browser.dom.window.dump.enabled", true);
pref("consoleservice.logcat", true);
pref("dom.activity.debug", true);
#endif

// Start the b2g in e10s mode same as the browser
pref("browser.tabs.remote.autostart", true);
pref("browser.tabs.remote.desktopbehavior", true);

// Bug 83793
pref("dom.ipc.forkserver.enable", true);

// Temporary disable checks that prevent loading https:// scripts from
// the system app, until nsContentSecurityManager.cpp stabilizes (eg. bug 1544011)
pref("dom.security.skip_remote_script_assertion_in_system_priv_context", true);

// Disable WebRender by default
pref("gfx.webrender.all", false);
pref("gfx.webrender.enabled", false);
pref("gfx.webrender.force-legacy-layers", true);
pref("gfx.webrender.picture-tile-width", 256);
pref("gfx.webrender.picture-tile-height", 512);

// We control process prelaunch from the embedding api.
pref("dom.ipc.processPrelaunch.enabled", false);

#ifdef TARGET_VARIANT_USERDEBUG
// Enable Marionette
pref("marionette.enabled", true);
#endif

// Enable IME API
pref("dom.inputmethod.enabled", true);

// MMS UA Profile settings
pref("wap.UAProf.url", "");
pref("wap.UAProf.tagname", "x-wap-profile");

// MMS version 1.1 = 0x11 (or decimal 17)
// MMS version 1.3 = 0x13 (or decimal 19)
// @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.34
pref("dom.mms.version", 19);

pref("dom.mms.requestStatusReport", true);

// Retrieval mode for MMS
// manual: Manual retrieval mode.
// automatic: Automatic retrieval mode even in roaming.
// automatic-home: Automatic retrieval mode in home network.
// never: Never retrieval mode.
pref("dom.mms.retrieval_mode", "manual");

pref("dom.mms.sendRetryCount", 3);
pref("dom.mms.sendRetryInterval", "10000,60000,180000");

pref("dom.mms.retrievalRetryCount", 4);
pref("dom.mms.retrievalRetryIntervals", "60000,300000,600000,1800000");

// KaiOS emoji font
pref("font.name-list.emoji", "KaiOS Emoji");

// Disable the path check in file system.
pref("dom.filesystem.pathcheck.disabled", true);

// Disable the native font source of freetype.
pref("gfx.font_rendering.native_font_source_none", true);

// KaiOS Captive Portal URL
pref("captivedetect.canonicalURL", "http://detectportal.kaiostech.com/success.txt");
pref("captivedetect.canonicalContent", "success");

pref("externalAPI.websocket.protocols", "kaios-services");
pref("externalAPI.websocket.url", "ws://localhost/");

//MVS featrue
#if B2G_CCUSTOM_MODULES == C001
pref("device.mvs", true);
#endif

pref("voice-input.enabled", false);
#if !defined(API_DAEMON_PORT) || API_DAEMON_PORT == 80
pref("voice-input.icon-url", "http://shared.localhost/style/voice-input/icons/voice-input.svg");
#else
pref("voice-input.icon-url", "http://shared.localhost:@API_DAEMON_PORT@/style/voice-input/icons/voice-input.svg");
#endif
pref("voice-input.supported-types", "text, search, url, tel, number, month, week");
pref("voice-input.excluded-x-inputmodes", "native, plain, simplified, spell");

pref("dom.storage.next_gen", false);

pref("dom.popup_allowed_events", "change click dblclick auxclick mouseup pointerup notificationclick reset submit touchend contextmenu keydown keyup");

pref("dom.hand-held-friendly.forceEnable", true);

pref("dom.webshare.enabled", true);

// Workaround https://bugzilla.mozilla.org/show_bug.cgi?id=1725339 until
// a better solution for b2g system app is found.
pref("security.disallow_privileged_https_subdocuments_loads", false);

// Workaround https://bugzilla.mozilla.org/show_bug.cgi?id=1735117 since
// the system app needs to load resources from http://shared.localhost
pref("security.disallow_privileged_https_stylesheet_loads", false);
pref("security.disallow_privileged_https_script_loads", false);

#ifdef MOZ_WIDGET_GONK
pref("b2g.api-daemon.uds-socket", "/dev/socket/api-daemon");
#else
pref("b2g.api-daemon.uds-socket", "/tmp/api-daemon-socket");
#endif
