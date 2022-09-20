/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_imeconnect_touchpal_IMEConnect_h
#define mozilla_dom_imeconnect_touchpal_IMEConnect_h

#include "nsCOMPtr.h"
#include "nsPIDOMWindow.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/File.h"

#include <string>
#include <list>
#include <vector>
#include <unordered_map>

extern "C" {
#include "dictionary_i.h"
}

#if defined(MOZ_WIDGET_GONK)
#  include <android/log.h>

#  define LOG_CT_TAG "TouchPalIME"

#  define CT_DEBUG 0

#  define CT_LOGI(args...) \
    __android_log_print(ANDROID_LOG_INFO, LOG_CT_TAG, ##args)
#  define CT_LOGW(args...) \
    __android_log_print(ANDROID_LOG_WARN, LOG_CT_TAG, ##args)
#  define CT_LOGE(args...) \
    __android_log_print(ANDROID_LOG_ERROR, LOG_CT_TAG, ##args)

#  if CT_DEBUG
#    define CT_LOGD(args...) \
      __android_log_print(ANDROID_LOG_DEBUG, LOG_CT_TAG, ##args)
#  else
#    define CT_LOGD(args...)
#  endif

#else

#  define CT_LOGD(args...)
#  define CT_LOGW(args...)
#  define CT_LOGE(args...)

#endif

#define IME_ID_MASK 0xFF
#define KEYBOARD_ID_MASK 0xFF00
#define KEYBOARD_ID_SHIFT 8

#define ERROR -1

#define CTO_RDWR 1
#define CTO_RDONLY 2

#define CT_INPUT_PRECISE_DEFAULT_PROB 0
#define CT_INPUT_CORRECTION_DEFAULT_PROB 0
#define CT_MAX_IMAGE_DESCRIPTOR 3
#define CT_DEFAULT_MAX_REQUEST 20

#define CUSTOM_HISTORY_SIZE 2

// the max buffer size per key, whose value is estimated from layout files
#define KEY_LAYOUT_MAX_BUF_SIZE 150
// the max letters number per key, whose value is estimated from layout files
#define KEY_LAYOUT_MAX_LETTERS 60

#define DICT_ROOT_PATH "/system/etc/dict/touchpal/"

using namespace std;

namespace mozilla {
namespace dom {

enum EImeId {
  eImeStart = 0x00,
  eImeFrenchCa = 0x01,
  eImeSpanishUs = 0x02,
  eImeSpanishMx = 0x03,
  eImeDanish = 0x04,
  eImeEstonian = 0x05,
  eImeFinnish = 0x06,
  eImeIcelandic = 0x07,
  eImeNorwegian = 0x08,
  eImeSwedish = 0x09,
  eImeIndonesian = 0x0A,
  eImeKhmer = 0x0B,
  eImeLao = 0x0C,
  eImeMalay = 0x0D,
  eImeSinhala = 0x0E,
  eImeTagalog = 0x0F,
  eImeThai = 0x10,
  eImeBurmeseZg = 0x11,
  eImeLatvian = 0x12,
  eImeLithuanian = 0x13,
  eImeUkrainian = 0x14,
  eImeAfrikaans = 0x15,
  eImeSwahili = 0x16,
  eImeZulu = 0x17,
  eImeFrenchAf = 0x18,
  eImePortugeseAf = 0x19,
  eImePortugeseBr = 0x1A,
  eImeSpanishAr = 0x1B,
  eImeAlbanian = 0x1C,
  eImeBosnianCy = 0x1D,
  eImeBosnian = 0x1E,
  eImeBulgarian = 0x1F,
  eImeCroatian = 0x20,
  eImeGreek = 0x21,
  eImeItalian = 0x22,
  eImeMacedonian = 0x23,
  eImePortugesePt = 0x24,
  eImeSerbian = 0x25,
  eImeSlovenian = 0x26,
  eImeSpanish = 0x27,
  eImeRomanian = 0x28,
  eImeBengaliBd = 0x29,
  eImeArmenian = 0x2A,
  eImeRussian = 0x2B,
  eImeTurkish = 0x2C,
  eImeEnglishUs = 0x2D,
  eImeRussianBy = 0x2E,
  eImeEnglish = 0x2F,
  eImeCzech = 0x30,
  eImeDutch = 0x31,
  eImeEnglishGb = 0x32,
  eImeFrenchFr = 0x33,
  eImeGerman = 0x34,
  eImeArabic = 0x35,
  eImeHebrew = 0x36,
  eImePersian = 0x37,
  eImeBengaliIn = 0x38,
  eImeHindi = 0x39,
  eImeTamil = 0x3A,
  eImeTelugu = 0x3B,
  eImeUrdu = 0x3C,
  eImeChinesePinyin = 0x3D,
  eImeChineseBihua = 0x3E,
  eImeChineseZhuyin = 0x3F,
  eImeHungarian = 0x40,
  eImePolish = 0x41,
  eImeSlovak = 0x42,
  eImeAzerbaijani = 0x43,
  eImeKazakh = 0x44,
  eImeGeorgian = 0x45,
  eImeXhosa = 0x46,
  eImeUzbek = 0x47,
  eImeKorean = 0x48,
  eImeAmharic = 0x49,
  eImeVietnamese = 0x4A,
  eImeBelarusian = 0x4B,
  eImeBasque = 0x4C,
  eImeGalician = 0x4D,
  eImeCatalonia = 0x4E,
  eImeJapanese = 0x4F,
  eImeHausa = 0x50,
  eImeSomali = 0x51,
  eImeTigre = 0x52,
  eImeOromo = 0x53,
  eImeEnd
};

const string SYS_DICT_FILE[eImeEnd] = {"",
                                       "french_ca.rom",
                                       "spanish_us.rom",
                                       "spanish_mx.rom",
                                       "danish.rom",
                                       "estonian.rom",
                                       "finnish.rom",
                                       "icelandic.rom",
                                       "norwegian.rom",
                                       "swedish.rom",
                                       "indonesian.rom",
                                       "khmer.rom",
                                       "lao.rom",
                                       "malaysian.rom",
                                       "sinhala.rom",
                                       "tagalog.rom",
                                       "thai.rom",
                                       "zawgyi.rom",
                                       "latvian.rom",
                                       "lithuanian.rom",
                                       "ukrainian.rom",
                                       "afrikaans.rom",
                                       "swahili.rom",
                                       "zulu.rom",
                                       "french_africa.rom",
                                       "portuguese_africa.rom",
                                       "portuguese_br.rom",
                                       "spanish_ar.rom",
                                       "albanian.rom",
                                       "bosnian_cyrillic.rom",
                                       "bosnian_latin.rom",
                                       "bulgarian.rom",
                                       "croatian.rom",
                                       "greek.rom",
                                       "italian.rom",
                                       "macedonian.rom",
                                       "portuguese_pt.rom",
                                       "serbian_latin.rom",
                                       "slovenian.rom",
                                       "spanish.rom",
                                       "romanian.rom",
                                       "bengali.rom",
                                       "armenian.rom",
                                       "russian.rom",
                                       "turkish.rom",
                                       "english_us.rom",
                                       "russian_by.rom",
                                       "english.rom",
                                       "czech.rom",
                                       "dutch.rom",
                                       "english_gb.rom",
                                       "french.rom",
                                       "german.rom",
                                       "arabian.rom",
                                       "hebrew.rom",
                                       "farsi.rom",
                                       "bengali_in.rom",
                                       "hindi.rom",
                                       "tamil.rom",
                                       "telugu.rom",
                                       "urdu.rom",
                                       "pinyin.rom",
                                       "bihua.rom",
                                       "zhuyin.rom",
                                       "hungarian.rom",
                                       "polish.rom",
                                       "slovak.rom",
                                       "azerbaijani.rom",
                                       "kazakh.rom",
                                       "georgian.rom",
                                       "xhosa.rom",
                                       "uzbek_cyrillic.rom",
                                       "korean.rom",
                                       "amharic.rom",
                                       "vietnamese.rom",
                                       "belarusian.rom",
                                       "basque.rom",
                                       "galician.rom",
                                       "catalonia.rom",
                                       "japanese.rom",
                                       "hausa.rom",
                                       "somali.rom",
                                       "tigre.rom",
                                       "oromo.rom"};

const string KEYLAYOUT_FILE[eImeEnd] = {"",
                                        "french.keys",
                                        "spanish.keys",
                                        "spanish.keys",
                                        "danish.keys",
                                        "estonian.keys",
                                        "finnish.keys",
                                        "icelandic.keys",
                                        "norwegian.keys",
                                        "swedish.keys",
                                        "indonesian.keys",
                                        "khmer.keys",
                                        "lao.keys",
                                        "malaysian.keys",
                                        "sinhala.keys",
                                        "tagalog.keys",
                                        "thai.keys",
                                        "zawgyi.keys",
                                        "latvian.keys",
                                        "lithuanian.keys",
                                        "ukrainian.keys",
                                        "afrikaans.keys",
                                        "swahili.keys",
                                        "zulu.keys",
                                        "french.keys",
                                        "portuguese.keys",
                                        "portuguese.keys",
                                        "spanish.keys",
                                        "albanian.keys",
                                        "bosnian_cyrillic.keys",
                                        "bosnian_latin.keys",
                                        "bulgarian.keys",
                                        "croatian.keys",
                                        "greek.keys",
                                        "italian.keys",
                                        "macedonian.keys",
                                        "portuguese.keys",
                                        "serbian_latin.keys",
                                        "slovenian.keys",
                                        "spanish.keys",
                                        "romanian.keys",
                                        "bengali.keys",
                                        "armenian.keys",
                                        "russian.keys",
                                        "turkish.keys",
                                        "english.keys",
                                        "russian.keys",
                                        "english.keys",
                                        "czech.keys",
                                        "dutch.keys",
                                        "english.keys",
                                        "french.keys",
                                        "german.keys",
                                        "arabian.keys",
                                        "hebrew.keys",
                                        "farsi.keys",
                                        "bengali_in.keys",
                                        "hindi.keys",
                                        "tamil.keys",
                                        "telugu.keys",
                                        "urdu.keys",
                                        "pinyin.keys",
                                        "bihua.keys",
                                        "zhuyin.keys",
                                        "hungarian.keys",
                                        "polish.keys",
                                        "slovak.keys",
                                        "azerbaijani.keys",
                                        "kazakh.keys",
                                        "georgian.keys",
                                        "xhosa.keys",
                                        "uzbek_cyrillic.keys",
                                        "korean.keys",
                                        "amharic.keys",
                                        "vietnamese.keys",
                                        "belarusian.keys",
                                        "basque.keys",
                                        "galician.keys",
                                        "catalonia.keys",
                                        "japanese.keys",
                                        "hausa.keys",
                                        "somali.keys",
                                        "tigre.keys",
                                        "oromo.keys"};

const string WESTERN_USR_DICT_FILE = "western.usr";

struct CharToVowel {
  char16_t c1;
  char16_t c2;
  char16_t v1;
  char16_t v2;
};

static const CharToVowel sKoreanVowels[] = {
    {.c1 = 0x318d, .c2 = 0x3163, .v1 = 0x3153, .v2 = 0x0000},  // ㆍㅣ -> ㅓ
    {.c1 = 0xff1a, .c2 = 0x3163, .v1 = 0x3155, .v2 = 0x0000},  // ：ㅣ -> ㅕ
    {.c1 = 0x318d, .c2 = 0x3161, .v1 = 0x3157, .v2 = 0x0000},  // ㆍㅡ -> ㅗ
    {.c1 = 0xff1a, .c2 = 0x3161, .v1 = 0x315b, .v2 = 0x0000},  // ：ㅡ -> ㅛ
    {.c1 = 0x318d, .c2 = 0x318d, .v1 = 0xff1a, .v2 = 0x0000},  // ㆍㆍ -> ：
    {.c1 = 0x3163, .c2 = 0x318d, .v1 = 0x314f, .v2 = 0x0000},  // ㅣㆍ -> ㅏ
    {.c1 = 0x314f, .c2 = 0x318d, .v1 = 0x3151, .v2 = 0x0000},  // ㅏㆍ -> ㅑ
    {.c1 = 0x3161, .c2 = 0x318d, .v1 = 0x315c, .v2 = 0x0000},  // ㅡㆍ -> ㅜ
    {.c1 = 0x315c, .c2 = 0x318d, .v1 = 0x3160, .v2 = 0x0000},  // ㅜㆍ -> ㅠ
    {.c1 = 0x3153, .c2 = 0x3163, .v1 = 0x3154, .v2 = 0x0000},  // ㅓㅣ -> ㅔ
    {.c1 = 0x3155, .c2 = 0x3163, .v1 = 0x3156, .v2 = 0x0000},  // ㅕㅣ -> ㅖ
    {.c1 = 0x314f, .c2 = 0x3163, .v1 = 0x3150, .v2 = 0x0000},  // ㅏㅣ -> ㅐ
    {.c1 = 0x3151, .c2 = 0x3163, .v1 = 0x3152, .v2 = 0x0000},  // ㅑㅣ -> ㅒ
    {.c1 = 0x3160, .c2 = 0x3163, .v1 = 0x315c, .v2 = 0x3153},  // ㅠㅣ -> ㅜㅓ
    {.c1 = 0x3163, .c2 = 0xff1a, .v1 = 0x3151, .v2 = 0x0000},  // ㅣ： -> ㅑ
    {.c1 = 0x3161, .c2 = 0xff1a, .v1 = 0x3160, .v2 = 0x0000},  // ㅡ： -> ㅠ
};

enum ESelectionBarId {
  eSelectionBarStart = 0,
  eSelectionBarWord = 1,
  eSelectionBarGroup = 2,
  eSelectionBarEnd
};

enum EKeyboardId {
  eKeyboardStart = 0,
  eKeyboardT9 = 1,
  eKeyboardQwerty = 2,
  eKeyboardEnd
};

enum EInitStatus {
  eInitStatusSuccess = 0,
  eInitStatusError = 1,
  eInitStatusDictNotFound = 2
};

struct DictFile {
  struct CT_BaseDictionary* dic;
  struct CT_BaseImageDescriptor* file_handles_base[CT_MAX_IMAGE_DESCRIPTOR];
  struct CT_LanguageImagesList images_list;
  int file_size;
};

class IMEConnect final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(IMEConnect)

  static already_AddRefed<IMEConnect> Constructor(const GlobalObject& aGlobal,
                                                  ErrorResult& aRv);

  static already_AddRefed<IMEConnect> Constructor(const GlobalObject& aGlobal,
                                                  uint32_t aLid,
                                                  ErrorResult& aRv);

  void GetName(nsAString& aName) const { aName.AssignLiteral("TouchPal"); }

  uint32_t CurrentLID() const {
    return mImeId | (mKeyboardId << KEYBOARD_ID_SHIFT);
  }

  uint16_t TotalWord() const { return mTotalWord; }

  void GetCandidateWord(nsAString& aResult) { aResult = mCandidateWord; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  nsCOMPtr<nsPIDOMWindowInner> mWindow;

  nsPIDOMWindowInner* GetOwner() const { return mWindow; }

  nsPIDOMWindowInner* GetParentObject() { return GetOwner(); }

  explicit IMEConnect(nsPIDOMWindowInner* aWindow);

  nsresult Init(uint32_t aLid);

  static bool HasSupport(JSContext* /* unused */, JSObject* aGlobal);
  static void SetLetter(const unsigned long aHexPrefix,
                        const unsigned long aHexLetter, ErrorResult& aRv);
  static void SetLetterMultiTap(const unsigned long aKeyCode,
                                const unsigned long aTapCount,
                                unsigned short aPrevUnichar);
  static void GetWordCandidates(const nsAString& aLetters,
                                const Sequence<nsString>& aContext,
                                nsAString& aRetval);
  static void GetNextWordCandidates(const nsAString& aWord, nsAString& aRetval);
  static void GetComposingWords(const nsAString& aLetters,
                                const long aIndicator, nsAString& aRetval);
  static void ImportDictionary(Blob& aBlob, ErrorResult& aRv);
  static uint32_t SetLanguage(const uint32_t aLid);

 private:
  ~IMEConnect();

  static int InitKeyLayout(uint32_t aImeId, uint32_t aKeyboardId);
  static int InitSysDictionary(uint32_t aImeId);
  static int DeinitSysDictionary();
  static void FetchCandidates();
  static void PackCandidates();
  static bool KoreanCharToVowel(char16_t c1, char16_t c2, char16_t* v1,
                                char16_t* v2);

  // callback function for input code expanding (aka ambiguous input)
  static ctint32 GetInputCodeExpandResult(
      ctint32 code, void* callbackToken,
      struct CT_InputCodeExpand* InputCodeExpand);

  static uint8_t mInitStatus;
  static uint8_t mImeId;
  static uint8_t mKeyboardId;
  static uint16_t mTotalWord;
  static uint16_t mTotalGroup;
  static uint16_t mActiveWordIndex;
  static uint16_t mActiveGroupIndex;
  static uint8_t mActiveSelectionBar;

  static nsString mCandidateWord;
  static list<u16string> mCandWord;
  static list<u16string> mCandGroup;
  static vector<wchar_t> mTypedKeycodes;
  static vector<u16string> mHistoryWords;
  static bool mHasPreciseCandidate;

  // maps key to letters
  static unordered_map<ctunicode_t, vector<ctunicode_t>> mKeyLetters;
  static struct DictFile mDictFile;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_imeconnect_touchpal_IMEConnect_h
