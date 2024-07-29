/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_hal_Types_h
#define mozilla_hal_Types_h

#include "ipc/EnumSerializer.h"
#include "mozilla/BitSet.h"
#include "mozilla/Observer.h"
#include "mozilla/dom/BatteryManagerBinding.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace hal {

/**
 * These constants specify special values for content process IDs.  You can get
 * a content process ID by calling ContentChild::GetID() or
 * ContentParent::GetChildID().
 */
const uint64_t CONTENT_PROCESS_ID_UNKNOWN = uint64_t(-1);
const uint64_t CONTENT_PROCESS_ID_MAIN = 0;

class SwitchEvent;

enum SwitchDevice {
  SWITCH_DEVICE_UNKNOWN = -1,
  SWITCH_HEADPHONES,
  SWITCH_USB,
  SWITCH_LINEOUT,
  NUM_SWITCH_DEVICE
};

enum SwitchState {
  SWITCH_STATE_UNKNOWN = -1,
  SWITCH_STATE_ON,
  SWITCH_STATE_OFF,
  SWITCH_STATE_HEADSET,    // Headphone with microphone
  SWITCH_STATE_HEADPHONE,  // without microphone
  SWITCH_STATE_LINEOUT,
  NUM_SWITCH_STATE
};

typedef Observer<SwitchEvent> SwitchObserver;

// Note that we rely on the order of this enum's entries.  Higher priorities
// should have larger int values.
enum ProcessPriority {
  PROCESS_PRIORITY_UNKNOWN = -1,
  PROCESS_PRIORITY_BACKGROUND,
  PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE,
  PROCESS_PRIORITY_FOREGROUND_KEYBOARD,
  // The special class for the preallocated process, high memory priority but
  // low CPU priority.
  PROCESS_PRIORITY_PREALLOC,
  // Any priority greater than or equal to FOREGROUND is considered
  // "foreground" for the purposes of priority testing, for example
  // CurrentProcessIsForeground().
  PROCESS_PRIORITY_FOREGROUND,
  PROCESS_PRIORITY_FOREGROUND_HIGH,
  PROCESS_PRIORITY_PARENT_PROCESS,
  NUM_PROCESS_PRIORITY
};

/**
 * Convert a ProcessPriority enum value to a string.  The strings returned by
 * this function are statically allocated; do not attempt to free one!
 *
 * If you pass an unknown process priority, we fatally assert in debug
 * builds and otherwise return "???".
 */
const char* ProcessPriorityToString(ProcessPriority aPriority);

/**
 * Used by ModifyWakeLock
 */
enum WakeLockControl {
  WAKE_LOCK_REMOVE_ONE = -1,
  WAKE_LOCK_NO_CHANGE = 0,
  WAKE_LOCK_ADD_ONE = 1,
  NUM_WAKE_LOCK
};

class FMRadioOperationInformation;

enum FMRadioOperation {
  FM_RADIO_OPERATION_UNKNOWN = -1,
  FM_RADIO_OPERATION_ENABLE,
  FM_RADIO_OPERATION_DISABLE,
  FM_RADIO_OPERATION_SEEK,
  FM_RADIO_OPERATION_TUNE,
  NUM_FM_RADIO_OPERATION
};

enum FMRadioOperationStatus {
  FM_RADIO_OPERATION_STATUS_UNKNOWN = -1,
  FM_RADIO_OPERATION_STATUS_SUCCESS,
  FM_RADIO_OPERATION_STATUS_FAIL,
  NUM_FM_RADIO_OPERATION_STATUS
};

enum FMRadioSeekDirection {
  FM_RADIO_SEEK_DIRECTION_UNKNOWN = -1,
  FM_RADIO_SEEK_DIRECTION_UP,
  FM_RADIO_SEEK_DIRECTION_DOWN,
  NUM_FM_RADIO_SEEK_DIRECTION
};

enum FMRadioBand {
  FM_USER_UNKNOWN_BAND = -1,
  FM_US_BAND,
  FM_EU_BAND,
  FM_JAPAN_STANDARD_BAND,
  FM_JAPAN_WIDE_BAND,
  FM_USER_DEFINED_BAND,
  NUM_FM_RADIO_BAND
};

enum FMRadioRDSStd {
  FM_RDS_STD_UNKNOWN = -1,
  FM_RDS_STD_RBDS,
  FM_RDS_STD_RDS,
  FM_RDS_STD_NONE,
  NUM_FM_RADIO_RDSSTD
};

enum FMRadioCountry {
  FM_RADIO_COUNTRY_UNKNOWN = -1,
  FM_RADIO_COUNTRY_US,  // USA
  FM_RADIO_COUNTRY_EU,
  FM_RADIO_COUNTRY_JP_STANDARD,
  FM_RADIO_COUNTRY_JP_WIDE,
  FM_RADIO_COUNTRY_DE,  // Germany
  FM_RADIO_COUNTRY_AW,  // Aruba
  FM_RADIO_COUNTRY_AU,  // Australlia
  FM_RADIO_COUNTRY_BS,  // Bahamas
  FM_RADIO_COUNTRY_BD,  // Bangladesh
  FM_RADIO_COUNTRY_CY,  // Cyprus
  FM_RADIO_COUNTRY_VA,  // Vatican
  FM_RADIO_COUNTRY_CO,  // Colombia
  FM_RADIO_COUNTRY_KR,  // Korea
  FM_RADIO_COUNTRY_DK,  // Denmark
  FM_RADIO_COUNTRY_EC,  // Ecuador
  FM_RADIO_COUNTRY_ES,  // Spain
  FM_RADIO_COUNTRY_FI,  // Finland
  FM_RADIO_COUNTRY_FR,  // France
  FM_RADIO_COUNTRY_GM,  // Gambia
  FM_RADIO_COUNTRY_HU,  // Hungary
  FM_RADIO_COUNTRY_IN,  // India
  FM_RADIO_COUNTRY_IR,  // Iran
  FM_RADIO_COUNTRY_IT,  // Italy
  FM_RADIO_COUNTRY_KW,  // Kuwait
  FM_RADIO_COUNTRY_LT,  // Lithuania
  FM_RADIO_COUNTRY_ML,  // Mali
  FM_RADIO_COUNTRY_MA,  // Morocco
  FM_RADIO_COUNTRY_NO,  // Norway
  FM_RADIO_COUNTRY_NZ,  // New Zealand
  FM_RADIO_COUNTRY_OM,  // Oman
  FM_RADIO_COUNTRY_PG,  // Papua New Guinea
  FM_RADIO_COUNTRY_NL,  // Netherlands
  FM_RADIO_COUNTRY_QA,  // Qatar
  FM_RADIO_COUNTRY_CZ,  // Czech Republic
  FM_RADIO_COUNTRY_UK,  // United Kingdom of Great Britain and Northern Ireland
  FM_RADIO_COUNTRY_RW,  // Rwandese Republic
  FM_RADIO_COUNTRY_SN,  // Senegal
  FM_RADIO_COUNTRY_SG,  // Singapore
  FM_RADIO_COUNTRY_SI,  // Slovenia
  FM_RADIO_COUNTRY_ZA,  // South Africa
  FM_RADIO_COUNTRY_SE,  // Sweden
  FM_RADIO_COUNTRY_CH,  // Switzerland
  FM_RADIO_COUNTRY_TW,  // Taiwan
  FM_RADIO_COUNTRY_TR,  // Turkey
  FM_RADIO_COUNTRY_UA,  // Ukraine
  FM_RADIO_COUNTRY_USER_DEFINED,
  NUM_FM_RADIO_COUNTRY
};

class FMRadioRDSGroup;
typedef Observer<FMRadioOperationInformation> FMRadioObserver;
typedef Observer<FMRadioRDSGroup> FMRadioRDSObserver;

/**
 * Represents a workload shared by a group of threads that should be completed
 * in a target duration each cycle.
 *
 * This is created using hal::CreatePerformanceHintSession(). Each cycle, the
 * actual work duration should be reported using ReportActualWorkDuration(). The
 * system can then adjust the scheduling accordingly in order to achieve the
 * target.
 */
class PerformanceHintSession {
 public:
  virtual ~PerformanceHintSession() = default;

  // Updates the session's target work duration for each cycle.
  virtual void UpdateTargetWorkDuration(TimeDuration aDuration) = 0;

  // Reports the session's actual work duration for a cycle.
  virtual void ReportActualWorkDuration(TimeDuration aDuration) = 0;
};

/**
 * Categorizes the CPUs on the system in to big, medium, and little classes.
 *
 * A set bit in each bitset indicates that the CPU of that index belongs to that
 * class. If the CPUs are fully homogeneous they are all categorized as big. If
 * there are only 2 classes, they are categorized as either big or little.
 * Finally, if there are >= 3 classes, the remainder will be categorized as
 * medium.
 *
 * If there are more than MAX_CPUS present we are unable to represent this
 * information.
 */
struct HeterogeneousCpuInfo {
  // We use a max of 32 because currently this is only implemented for Android
  // where we are unlikely to need more CPUs than that, and it simplifies
  // dealing with cpu_set_t as CPU_SETSIZE is 32 on 32-bit Android.
  static const size_t MAX_CPUS = 32;
  size_t mTotalNumCpus;
  mozilla::BitSet<MAX_CPUS> mLittleCpus;
  mozilla::BitSet<MAX_CPUS> mMediumCpus;
  mozilla::BitSet<MAX_CPUS> mBigCpus;
};

}  // namespace hal
}  // namespace mozilla

namespace IPC {

/**
 * WakeLockControl serializer.
 */
template <>
struct ParamTraits<mozilla::hal::WakeLockControl>
    : public ContiguousEnumSerializer<mozilla::hal::WakeLockControl,
                                      mozilla::hal::WAKE_LOCK_REMOVE_ONE,
                                      mozilla::hal::NUM_WAKE_LOCK> {};

/**
 * Serializer for SwitchState
 */
template <>
struct ParamTraits<mozilla::hal::SwitchState>
    : public ContiguousEnumSerializer<mozilla::hal::SwitchState,
                                      mozilla::hal::SWITCH_STATE_UNKNOWN,
                                      mozilla::hal::NUM_SWITCH_STATE> {};

/**
 * Serializer for SwitchDevice
 */
template <>
struct ParamTraits<mozilla::hal::SwitchDevice>
    : public ContiguousEnumSerializer<mozilla::hal::SwitchDevice,
                                      mozilla::hal::SWITCH_DEVICE_UNKNOWN,
                                      mozilla::hal::NUM_SWITCH_DEVICE> {};

template <>
struct ParamTraits<mozilla::hal::ProcessPriority>
    : public ContiguousEnumSerializer<mozilla::hal::ProcessPriority,
                                      mozilla::hal::PROCESS_PRIORITY_UNKNOWN,
                                      mozilla::hal::NUM_PROCESS_PRIORITY> {};

/**
 * Serializer for FMRadioOperation
 */
template <>
struct ParamTraits<mozilla::hal::FMRadioOperation>
    : public ContiguousEnumSerializer<mozilla::hal::FMRadioOperation,
                                      mozilla::hal::FM_RADIO_OPERATION_UNKNOWN,
                                      mozilla::hal::NUM_FM_RADIO_OPERATION> {};

/**
 * Serializer for FMRadioOperationStatus
 */
template <>
struct ParamTraits<mozilla::hal::FMRadioOperationStatus>
    : public ContiguousEnumSerializer<
          mozilla::hal::FMRadioOperationStatus,
          mozilla::hal::FM_RADIO_OPERATION_STATUS_UNKNOWN,
          mozilla::hal::NUM_FM_RADIO_OPERATION_STATUS> {};

/**
 * Serializer for FMRadioSeekDirection
 */
template <>
struct ParamTraits<mozilla::hal::FMRadioSeekDirection>
    : public ContiguousEnumSerializer<
          mozilla::hal::FMRadioSeekDirection,
          mozilla::hal::FM_RADIO_SEEK_DIRECTION_UNKNOWN,
          mozilla::hal::NUM_FM_RADIO_SEEK_DIRECTION> {};

/**
 * Serializer for FMRadioBand
 **/
template <>
struct ParamTraits<mozilla::hal::FMRadioBand>
    : public ContiguousEnumSerializer<mozilla::hal::FMRadioBand,
                                      mozilla::hal::FM_USER_UNKNOWN_BAND,
                                      mozilla::hal::NUM_FM_RADIO_BAND> {};

/**
 * Serializer for FMRadioRDSStd
 **/
template <>
struct ParamTraits<mozilla::hal::FMRadioRDSStd>
    : public ContiguousEnumSerializer<mozilla::hal::FMRadioRDSStd,
                                      mozilla::hal::FM_RDS_STD_UNKNOWN,
                                      mozilla::hal::NUM_FM_RADIO_RDSSTD> {};

/**
 * Serializer for FMRadioCountry
 **/
template <>
struct ParamTraits<mozilla::hal::FMRadioCountry>
    : public ContiguousEnumSerializer<mozilla::hal::FMRadioCountry,
                                      mozilla::hal::FM_RADIO_COUNTRY_UNKNOWN,
                                      mozilla::hal::NUM_FM_RADIO_COUNTRY> {};

/*
 * Serializer for BatteryHealth
 */
template <>
struct ParamTraits<mozilla::dom::BatteryHealth>
    : public ContiguousEnumSerializer<mozilla::dom::BatteryHealth,
                                      mozilla::dom::BatteryHealth::Good,
                                      mozilla::dom::BatteryHealth::EndGuard_> {
};

}  // namespace IPC

#endif  // mozilla_hal_Types_h
