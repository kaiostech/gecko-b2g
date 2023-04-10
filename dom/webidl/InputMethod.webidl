/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Exposed=Window, Pref="dom.inputmethod.enabled", Func="B2G::HasInputPermission"]
interface InputMethod {

  /**
   * Optional offset and length represents the highlight range in this text.
   * @param offset The offset from the beginning of text. Defaults to 0.
   * @param length The length of characters to highlight. Defaults to 0.
   */
  Promise<boolean> setComposition(DOMString text,
                                  optional long offset,
                                  optional long length);

  Promise<boolean> endComposition(optional DOMString text);

  Promise<boolean> sendKey(DOMString key);
  Promise<boolean> keydown(DOMString key);
  Promise<boolean> keyup(DOMString key);

  /**
   * Delete a text/node/selection before the cursor
   */
  Promise<boolean> deleteBackward();

  /**
   * Remove focus from the current input, usable by Gaia System app, globally,
   * regardless of the current focus state.
   */
  undefined removeFocus();

  Promise<sequence<long>> getSelectionRange();

  /**
   * Get the whole text content of the input field.
   * @return DOMString
   */
  Promise<DOMString> getText(optional long offset, optional long length);

  /**
   * Set the value on the currently focused element. This has to be used
   * for special situations where the value had to be chosen amongst a
   * list (type=month) or a widget (type=date, time, etc.) or contenteditable.
   * If the value passed in parameter isn't valid (in the term of HTML5
   * Forms Validation), the value will simply be ignored by the element.
   */
  undefined setValue(DOMString value);

  // Select and delete all editable content including selection|text|node.
  undefined clearAll();

  /**
   * Commit text to current input field and replace text around
   * cursor position. It will clear the current composition.
   *
   * @param text The string to be replaced with.
   * @param offset The offset from the cursor position where replacing starts. Defaults to 0.
   * @param length The length of text to replace. Defaults to 0.
   * @return boolean
   */
  Promise<boolean> replaceSurroundingText(DOMString text, optional long offset, optional long length);


  };
