/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Exposed=Window, Func="IMEConnect::HasSupport"]
interface IMEConnect {
  [Throws]
  constructor();

  [Throws]
  constructor(unsigned long lid);

  unsigned long setLanguage(unsigned long lid);

  [Throws]
  undefined setLetter (unsigned long hexPrefix, unsigned long hexLetter);

  undefined setLetterMultiTap(unsigned long keyCode, unsigned long tapCount, unsigned short prevUnichar);

  DOMString getWordCandidates(DOMString letters, optional sequence<DOMString> context = []);

  DOMString getNextWordCandidates(DOMString word);

  DOMString getComposingWords(DOMString letters, long indicator);

  [Throws]
  undefined importDictionary(Blob dictionary);

  readonly attribute DOMString candidateWord;

  readonly attribute short totalWord;

  readonly attribute unsigned long currentLID;

  readonly attribute DOMString name;
};
