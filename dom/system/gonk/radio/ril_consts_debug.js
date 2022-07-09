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

// Set to true to debug all RIL layers
this.DEBUG_ALL = false;

// Set individually to debug specific layers
this.DEBUG_WORKER = DEBUG_ALL;
this.DEBUG_CONTENT_HELPER = DEBUG_ALL;
this.DEBUG_RIL = DEBUG_ALL;

this.PREF_RIL_DEBUG_ENABLED = "ril.debugging.enabled";
this.PREF_RIL_WORKER_DEBUG_ENABLED = "ril.worker.debugging.enabled";

// Allow this file to be imported via Components.utils.import().
const EXPORTED_SYMBOLS = Object.keys(this);
