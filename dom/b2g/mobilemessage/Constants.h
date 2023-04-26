/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_Constants_h
#define mozilla_dom_mobilemessage_Constants_h

namespace mozilla {
namespace dom {
namespace mobilemessage {

// Defined in the .cpp.
extern const char* kSmsReceivedObserverTopic;
extern const char* kSmsRetrievingObserverTopic;
extern const char* kSmsSendingObserverTopic;
extern const char* kSmsSentObserverTopic;
extern const char* kSmsFailedObserverTopic;
extern const char* kSmsDeliverySuccessObserverTopic;
extern const char* kSmsDeliveryErrorObserverTopic;
extern const char* kSilentSmsReceivedObserverTopic;
extern const char* kSmsReadSuccessObserverTopic;
extern const char* kSmsReadErrorObserverTopic;
extern const char* kSmsDeletedObserverTopic;

#define DELIVERY_RECEIVED u"received"_ns
#define DELIVERY_SENDING u"sending"_ns
#define DELIVERY_SENT u"sent"_ns
#define DELIVERY_ERROR u"error"_ns
#define DELIVERY_NOT_DOWNLOADED u"not-downloaded"_ns

#define DELIVERY_STATUS_NOT_APPLICABLE u"not-applicable"_ns
#define DELIVERY_STATUS_SUCCESS u"success"_ns
#define DELIVERY_STATUS_PENDING u"pending"_ns
#define DELIVERY_STATUS_ERROR u"error"_ns
#define DELIVERY_STATUS_REJECTED u"rejected"_ns
#define DELIVERY_STATUS_MANUAL u"manual"_ns

#define READ_STATUS_NOT_APPLICABLE u"not-applicable"_ns
#define READ_STATUS_SUCCESS u"success"_ns
#define READ_STATUS_PENDING u"pending"_ns
#define READ_STATUS_ERROR u"error"_ns

#define MESSAGE_CLASS_NORMAL u"normal"_ns
#define MESSAGE_CLASS_CLASS_0 u"class-0"_ns
#define MESSAGE_CLASS_CLASS_1 u"class-1"_ns
#define MESSAGE_CLASS_CLASS_2 u"class-2"_ns
#define MESSAGE_CLASS_CLASS_3 u"class-3"_ns

#define MESSAGE_TYPE_SMS u"sms"_ns
#define MESSAGE_TYPE_MMS u"mms"_ns

#define ATTACHMENT_STATUS_NONE u"none"_ns
#define ATTACHMENT_STATUS_NOT_DOWNLOADED u"not-downloaded"_ns
#define ATTACHMENT_STATUS_DOWNLOADED u"downloaded"_ns

}  // namespace mobilemessage
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_mobilemessage_Constants_h
