/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[Pref="dom.mobileconnection.enabled",
 Exposed=Window]
interface MobileCellInfo
{
  /**
   * Mobile Location Area Code (LAC) for GSM/WCDMA networks.
   *
   * Possible ranges from 0x0000 to 0xffff.
   * -1 if the LAC is unknown.
   */
  readonly attribute long gsmLocationAreaCode;

  /**
   * Mobile Cell ID for GSM/WCDMA networks.
   *
   * Possible ranges from 0x00000000 to 0xffffffff.
   * -1 if the cell id is unknown.
   */
  readonly attribute long long gsmCellId;

  /**
   * Base Station ID for CDMA networks.
   *
   * Possible ranges from 0 to 65535.
   * -1 if the base station id is unknown.
   */
  readonly attribute long cdmaBaseStationId;

  /**
   * Base Station Latitude for CDMA networks.
   *
   * Possible ranges from -1296000 to 1296000.
   * -2147483648 if the latitude is unknown.
   *
   * @see 3GPP2 C.S0005-A v6.0.
   */
  readonly attribute long cdmaBaseStationLatitude;

  /**
   * Base Station Longitude for CDMA networks.
   *
   * Possible ranges from -2592000 to 2592000.
   * -2147483648 if the longitude is unknown.
   *
   * @see 3GPP2 C.S0005-A v6.0.
   */
  readonly attribute long cdmaBaseStationLongitude;

  /**
   * System ID for CDMA networks.
   *
   * Possible ranges from 0 to 32767.
   * -1 if the system id is unknown.
   */
  readonly attribute long cdmaSystemId;

  /**
   * Network ID for CDMA networks.
   *
   * Possible ranges from 0 to 65535.
   * -1 if the network id is unknown.
   */
  readonly attribute long cdmaNetworkId;

  /**
   * The TSB-58 Roaming Indicator on a registered CDMA or EVDO system.
   *
   * Possible ranges from 0 to 255.
   * -1 if the indicator is unknown.
   *
   * @see 3GPP2 C.R1001-D v1.0.
   */
  readonly attribute short cdmaRoamingIndicator;

  /**
   * The default Roaming Indicator from PRL if registered on a CDMA or EVDO system.
   *
   * Possible ranges from 0 to 255.
   * -1 if the indicator is unknown.
   */
  readonly attribute short cdmaDefaultRoamingIndicator;

  /**
   * The indicator of whether the current system is in the PRL
   * if registered on a CDMA or EVDO system.
   */
  readonly attribute boolean cdmaSystemIsInPRL;

  /**
   * The tracking area code for LTE or NR
   */
  readonly attribute long tac;

  /**
   * The Cell Identity for LTE or NR
   */
  readonly attribute long long ci;

  /**
   * The Physical Cell Identity for LTE or NR
   */
  readonly attribute long long pci;

  /**
   * The Absolute Radio Frequency Channel Number for LTE or NR
   */
  readonly attribute long arfcns;

  // Eutran bands mask
  const unsigned long long EBAND_1 = 0x1; //1 << 0
  const unsigned long long EBAND_2 = 0x2; //1 << 1
  const unsigned long long EBAND_3 = 0x4; //1 << 2
  const unsigned long long EBAND_4 = 0x8; //1 << 3
  const unsigned long long EBAND_5 = 0x10; //1 << 4
  const unsigned long long EBAND_6 = 0x20; //1 << 5
  const unsigned long long EBAND_7 = 0x40; //1 << 6
  const unsigned long long EBAND_8 = 0x80; //1 << 7
  const unsigned long long EBAND_9 = 0x100; //1 << 8
  const unsigned long long EBAND_10 = 0x200; //1 << 9
  const unsigned long long EBAND_11 = 0x400; //1 << 10
  const unsigned long long EBAND_12 = 0x800; //1 << 11
  const unsigned long long EBAND_13 = 0x1000; //1 << 12
  const unsigned long long EBAND_14 = 0x2000; //1 << 13
  const unsigned long long EBAND_17 = 0x4000; //1 << 14
  const unsigned long long EBAND_18 = 0x8000; //1 << 15
  const unsigned long long EBAND_19 = 0x10000; //1 << 16
  const unsigned long long EBAND_20 = 0x20000; //1 << 17
  const unsigned long long EBAND_21 = 0x40000; //1 << 18
  const unsigned long long EBAND_22 = 0x80000; //1 << 19
  const unsigned long long EBAND_23 = 0x100000; //1 << 20
  const unsigned long long EBAND_24 = 0x200000; //1 << 21
  const unsigned long long EBAND_25 = 0x400000; //1 << 22
  const unsigned long long EBAND_26 = 0x800000; //1 << 23
  const unsigned long long EBAND_27 = 0x1000000; //1 << 24
  const unsigned long long EBAND_28 = 0x2000000; //1 << 25
  const unsigned long long EBAND_30 = 0x4000000; //1 << 26
  const unsigned long long EBAND_31 = 0x8000000; //1 << 27
  const unsigned long long EBAND_33 = 0x10000000; //1 << 28
  const unsigned long long EBAND_34 = 0x20000000; //1 << 29
  const unsigned long long EBAND_35 = 0x40000000; //1 << 30
  const unsigned long long EBAND_36 = 0x80000000; //1 << 31
  const unsigned long long EBAND_37 = 0x100000000; //1 << 32;
  const unsigned long long EBAND_38 = 0x200000000; //1 << 33;
  const unsigned long long EBAND_39 = 0x400000000; //1 << 34;
  const unsigned long long EBAND_40 = 0x800000000; //1 << 35;
  const unsigned long long EBAND_41 = 0x1000000000; //1 << 36;
  const unsigned long long EBAND_42 = 0x2000000000; //1 << 37;
  const unsigned long long EBAND_43 = 0x4000000000; //1 << 38;
  const unsigned long long EBAND_44 = 0x8000000000; //1 << 39;
  const unsigned long long EBAND_45 = 0x10000000000; //1 << 40;
  const unsigned long long EBAND_46 = 0x20000000000; //1 << 41;
  const unsigned long long EBAND_47 = 0x40000000000; //1 << 42;
  const unsigned long long EBAND_48 = 0x80000000000; //1 << 43;
  const unsigned long long EBAND_49 = 0x100000000000; //1 << 44;
  const unsigned long long EBAND_50 = 0x200000000000; //1 << 45;
  const unsigned long long EBAND_51 = 0x400000000000; //1 << 46;
  const unsigned long long EBAND_52 = 0x800000000000; //1 << 47;
  const unsigned long long EBAND_53 = 0x1000000000000; //1 << 48;
  const unsigned long long EBAND_65 = 0x2000000000000; //1 << 49;
  const unsigned long long EBAND_66 = 0x4000000000000; //1 << 50;
  const unsigned long long EBAND_68 = 0x8000000000000; //1 << 51;
  const unsigned long long EBAND_70 = 0x10000000000000; //1 << 52;
  const unsigned long long EBAND_71 = 0x20000000000000; //1 << 53;
  const unsigned long long EBAND_72 = 0x40000000000000; //1 << 54;
  const unsigned long long EBAND_73 = 0x80000000000000; //1 << 55;
  const unsigned long long EBAND_74 = 0x100000000000000; //1 << 56;
  const unsigned long long EBAND_85 = 0x200000000000000; //1 << 57;
  const unsigned long long EBAND_87 = 0x400000000000000; //1 << 58;
  const unsigned long long EBAND_88 = 0x800000000000000; //1 << 59;

  // Ngran bands mask
  const unsigned long long NBAND_1 = 0x1; //1 << 0;
  const unsigned long long NBAND_2 = 0x2; //1 << 1;
  const unsigned long long NBAND_3 = 0x4; //1 << 2;
  const unsigned long long NBAND_5 = 0x8; //1 << 3;
  const unsigned long long NBAND_7 = 0x10; //1 << 4;
  const unsigned long long NBAND_8 = 0x20; //1 << 5;
  const unsigned long long NBAND_12 = 0x40; //1 << 6;
  const unsigned long long NBAND_14 = 0x80; //1 << 7;
  const unsigned long long NBAND_18 = 0x100; //1 << 8;
  const unsigned long long NBAND_20 = 0x200; //1 << 9;
  const unsigned long long NBAND_25 = 0x400; //1 << 10;
  const unsigned long long NBAND_26 = 0x800; //1 << 11;
  const unsigned long long NBAND_28 = 0x1000; //1 << 12;
  const unsigned long long NBAND_29 = 0x2000; //1 << 13;
  const unsigned long long NBAND_30 = 0x4000; //1 << 14;
  const unsigned long long NBAND_34 = 0x8000; //1 << 15;
  const unsigned long long NBAND_38 = 0x10000; //1 << 16;
  const unsigned long long NBAND_39 = 0x20000; //1 << 17;
  const unsigned long long NBAND_40 = 0x40000; //1 << 18;
  const unsigned long long NBAND_41 = 0x80000; //1 << 19;
  const unsigned long long NBAND_46 = 0x100000; //1 << 20;
  const unsigned long long NBAND_48 = 0x200000; //1 << 21;
  const unsigned long long NBAND_50 = 0x400000; //1 << 22;
  const unsigned long long NBAND_51 = 0x800000; //1 << 23;
  const unsigned long long NBAND_53 = 0x1000000; //1 << 24;
  const unsigned long long NBAND_65 = 0x2000000; //1 << 25;
  const unsigned long long NBAND_66 = 0x4000000; //1 << 26;
  const unsigned long long NBAND_70 = 0x8000000; //1 << 27;
  const unsigned long long NBAND_71 = 0x10000000; //1 << 28;
  const unsigned long long NBAND_74 = 0x20000000; //1 << 29;
  const unsigned long long NBAND_75 = 0x40000000; //1 << 30;
  const unsigned long long NBAND_76 = 0x80000000; //1 << 31;
  const unsigned long long NBAND_77 = 0x100000000; //1 << 32;
  const unsigned long long NBAND_78 = 0x200000000; //1 << 33;
  const unsigned long long NBAND_79 = 0x400000000; //1 << 34;
  const unsigned long long NBAND_80 = 0x800000000; //1 << 35;
  const unsigned long long NBAND_81 = 0x1000000000; //1 << 36;
  const unsigned long long NBAND_82 = 0x2000000000; //1 << 37;
  const unsigned long long NBAND_83 = 0x4000000000; //1 << 38;
  const unsigned long long NBAND_84 = 0x8000000000; //1 << 39;
  const unsigned long long NBAND_86 = 0x10000000000; //1 << 40;
  const unsigned long long NBAND_89 = 0x20000000000; //1 << 41;
  const unsigned long long NBAND_90 = 0x40000000000; //1 << 42;
  const unsigned long long NBAND_91 = 0x80000000000; //1 << 43;
  const unsigned long long NBAND_92 = 0x100000000000; //1 << 44;
  const unsigned long long NBAND_93 = 0x200000000000; //1 << 45;
  const unsigned long long NBAND_94 = 0x400000000000; //1 << 46;
  const unsigned long long NBAND_95 = 0x800000000000; //1 << 47;
  const unsigned long long NBAND_96 = 0x1000000000000; //1 << 48;
  const unsigned long long NBAND_257 = 0x2000000000000; //1 << 49;
  const unsigned long long NBAND_258 = 0x4000000000000; //1 << 50;
  const unsigned long long NBAND_260 = 0x8000000000000; //1 << 51;
  const unsigned long long NBAND_261 = 0x10000000000000; //1 << 52;
  /**
   * The Bands used by the cell for LTE or NR
   */
  readonly attribute long long bands;
};
