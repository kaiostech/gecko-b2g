/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
[Func="B2G::HasEUiccSupport", Exposed=Window]
interface EUiccManager : EventTarget {
  /**
   * To get the current the id of EUICC.
   *
   * @return DOMString.
   */
  [Throws]
  Promise<DOMString> getEid();


  [Throws]
  Promise<DOMString> GetEuiccInfo2();

  /**
   * To add profile according to activation code and confirmation code from carrier
   *
   * @param activationCode
   *        activation code from carrier or QR code.
   * @param confirmCode
   *        confirmation code from carrier or QR code.
   *
   * @return a Promise
   *         If succeeds, the profile is add and downloaded to euicc. Otherwise, the reject
   *         will give a fail reason.
   */
  [Throws]
  Promise<undefined> addProfile(DOMString activationCode, DOMString confirmCode);

  /**
   *
   * @param iccId
   *        iccId in the EUiccPorifleInfo of the profile to delete .
   *
   * @return a Promise
   *         If succeeds, the profile is deleted. Otherwise, the reject
   *         will give a fail reason.
   */
  [Throws]
  Promise<undefined> deleteProfile(DOMString iccId);

  /**
   * To get all prfoiles in euicc
   *
   * @return a Promise
   *         If succeeds, the EUiccPorifleInfo list will be given. Otherwise, the reject
   *         means fail.
   */
  [Throws]
  Promise<undefined> queryProfiles();

  /**
   *
   * @param iccId
   *        iccId in the EUiccPorifleInfo of the profile to enable .
   *
   * @return a Promise
   *         If succeeds, the profile is enabled. Otherwise, the reject
   *         will give a boolean to show  it's old status.
   */
  [Throws]
  Promise<undefined> enableProfile(DOMString iccId);

  /**
   *
   * @param iccId
   *        iccId in the EUiccPorifleInfo of the profile to disable .
   *
   * @return a Promise
   *         If succeeds, the profile is disabled. Otherwise, the reject
   *         will give a boolean to show  it's old status.
   */
  [Throws]
  Promise<undefined> disableProfile(DOMString iccId);

  /**
   *
   * @param nickName
   *        new nickName for profile.
   * @param nickName
   *        iccId of the profile to update nick name.
   *
   * @return a Promise
   *         If succeeds, the profile nickname is updated. Otherwise, the reject
   *         will give a reason.
   */
  [Throws]
  Promise<undefined> updateProfileNickName(DOMString iccId , DOMString nickName);

  /**
   *  To delete all profiles
   * @param resetMask
   *        new nickName for profile.
   *
   * @return a Promise
   *         If succeeds, all profiles are deleted. Otherwise, the reject
   *         means fail.
   */
  [Throws]
  Promise<undefined> resetMemory(unsigned long resetMask);

  /**
   * To tell modem the user consent result
   * @param userOk
   *        True means user allow this action.

   * @return a Promise
   *         If succeeds, all profiles are deleted. Otherwise, the reject
   *         means fail.
   */
  [Throws]
  Promise<undefined> setUserConsent(boolean userOk);

  /**
   * To set SM-DP address
   * @param addr
   *       The addr of SM-DP in SGP.21_v2.2.

   * @return a Promise
   *         If succeeds, the sm-dp address in euicc is updated. Otherwise, the reject
   *         means fail.
   */
  [Throws]
  Promise<undefined> setSMDPAddr(DOMString addr);

  /**
   * To get SM-DP address in euicc
   * @param addr
   *       The addr of SM-DP in SGP.21_v2.2.

   * @return a Promise
   *         If succeeds, the addr of SM-DP will be given as a string. Otherwise, the reject
   *         means fail.
   */
  [Throws]
  Promise<DOMString> getSMDPAddr();

  /**
   * Add pofile progress indication message
   */
  attribute EventHandler onindicationreceived;
};
