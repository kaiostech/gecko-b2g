/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

// Copied from ril_consts.js. Some entries are commented out in ril_const.js,
// but we still want to test them here.
const GSM_SMS_STRICT_7BIT_CHARMAP = {
  $: "\u0024", // "$" => "$", already in default alphabet
  "\u00a5": "\u00a5", // "¥" => "¥", already in default alphabet
  À: "\u0041", // "À" => "A"
  Á: "\u0041", // "Á" => "A"
  Â: "\u0041", // "Â" => "A"
  Ã: "\u0041", // "Ã" => "A"
  Ä: "\u00c4", // "Ä" => "Ä", already in default alphabet
  Å: "\u00c5", // "Å" => "Å", already in default alphabet
  Æ: "\u00c6", // "Æ" => "Æ", already in default alphabet
  Ç: "\u00c7", // "Ç" => "Ç", already in default alphabet
  È: "\u0045", // "È" => "E"
  É: "\u00c9", // "É" => "É", already in default alphabet
  Ê: "\u0045", // "Ê" => "E"
  Ë: "\u0045", // "Ë" => "E"
  Ì: "\u0049", // "Ì" => "I"
  Í: "\u0049", // "Í" => "I"
  Î: "\u0049", // "Î" => "I"
  Ï: "\u0049", // "Ï" => "I"
  Ñ: "\u00d1", // "Ñ" => "Ñ", already in default alphabet
  Ò: "\u004f", // "Ò" => "O"
  Ó: "\u004f", // "Ó" => "O"
  Ô: "\u004f", // "Ô" => "O"
  Õ: "\u004f", // "Õ" => "O"
  Ö: "\u00d6", // "Ö" => "Ö", already in default alphabet
  Ù: "\u0055", // "Ù" => "U"
  Ú: "\u0055", // "Ú" => "U"
  Û: "\u0055", // "Û" => "U"
  Ü: "\u00dc", // "Ü" => "Ü", already in default alphabet
  ß: "\u00df", // "ß" => "ß", already in default alphabet
  à: "\u00e0", // "à" => "à", already in default alphabet
  á: "\u0061", // "á" => "a"
  â: "\u0061", // "â" => "a"
  ã: "\u0061", // "ã" => "a"
  ä: "\u00e4", // "ä" => "ä", already in default alphabet
  å: "\u00e5", // "å" => "å", already in default alphabet
  æ: "\u00e6", // "æ" => "æ", already in default alphabet
  ç: "\u00c7", // "ç" => "Ç"
  è: "\u00e8", // "è" => "è", already in default alphabet
  é: "\u00e9", // "é" => "é", already in default alphabet
  ê: "\u0065", // "ê" => "e"
  ë: "\u0065", // "ë" => "e"
  ì: "\u00ec", // "ì" => "ì", already in default alphabet
  í: "\u0069", // "í" => "i"
  î: "\u0069", // "î" => "i"
  ï: "\u0069", // "ï" => "i"
  ñ: "\u00f1", // "ñ" => "ñ", already in default alphabet
  ò: "\u00f2", // "ò" => "ò", already in default alphabet
  ó: "\u006f", // "ó" => "o"
  ô: "\u006f", // "ô" => "o"
  õ: "\u006f", // "õ" => "o"
  ö: "\u00f6", // "ö" => "ö", already in default alphabet
  ø: "\u00f8", // "ø" => "ø", already in default alphabet
  ù: "\u00f9", // "ù" => "ù", already in default alphabet
  ú: "\u0075", // "ú" => "u"
  û: "\u0075", // "û" => "u"
  ü: "\u00fc", // "ü" => "ü", already in default alphabet
  þ: "\u0074", // "þ" => "t"
  Ā: "\u0041", // "Ā" => "A"
  ā: "\u0061", // "ā" => "a"
  Ć: "\u0043", // "Ć" => "C"
  ć: "\u0063", // "ć" => "c"
  Č: "\u0043", // "Č" => "C"
  č: "\u0063", // "č" => "c"
  ď: "\u0064", // "ď" => "d"
  Đ: "\u0044", // "Đ" => "D"
  đ: "\u0064", // "đ" => "d"
  Ē: "\u0045", // "Ē" => "E"
  ē: "\u0065", // "ē" => "e"
  Ę: "\u0045", // "Ę" => "E"
  ę: "\u0065", // "ę" => "e"
  Ĩ: "\u0049", // "Ĩ" => "I"
  ĩ: "\u0069", // "ĩ" => "i"
  Ī: "\u0049", // "Ī" => "I"
  ī: "\u0069", // "ī" => "i"
  Į: "\u0049", // "Į" => "I"
  į: "\u0069", // "į" => "i"
  Ł: "\u004c", // "Ł" => "L"
  ł: "\u006c", // "ł" => "l"
  Ń: "\u004e", // "Ń" => "N"
  ń: "\u006e", // "ń" => "n"
  Ň: "\u004e", // "Ň" => "N"
  ň: "\u006e", // "ň" => "n"
  Ō: "\u004f", // "Ō" => "O"
  ō: "\u006f", // "ō" => "o"
  Œ: "\u004f", // "Œ" => "O"
  œ: "\u006f", // "œ" => "o"
  Ř: "\u0052", // "Ř" => "R"
  ř: "\u0072", // "ř" => "r"
  Š: "\u0053", // "Š" => "S"
  š: "\u0073", // "š" => "s"
  ť: "\u0074", // "ť" => "t"
  Ũ: "\u0055", // "Ū" => "U"
  ũ: "\u0075", // "ū" => "u"
  Ū: "\u0055", // "Ū" => "U"
  ū: "\u0075", // "ū" => "u"
  Ÿ: "\u0059", // "Ÿ" => "Y"
  Ź: "\u005a", // "Ź" => "Z"
  ź: "\u007a", // "ź" => "z"
  Ż: "\u005a", // "Ż" => "Z"
  ż: "\u007a", // "ż" => "z"
  Ž: "\u005a", // "Ž" => "Z"
  ž: "\u007a", // "ž" => "z"
  ɛ: "\u0045", // "ɛ" => "E"
  Θ: "\u0398", // "Θ" => "Θ", already in default alphabet
  Ṽ: "\u0056", // "Ṽ" => "V"
  ṽ: "\u0076", // "ṽ" => "v"
  Ẽ: "\u0045", // "Ẽ" => "E"
  ẽ: "\u0065", // "ẽ" => "e"
  Ỹ: "\u0059", // "Ỹ" => "Y"
  ỹ: "\u0079", // "ỹ" => "y"
  "\u20a4": "\u00a3", // "₤" => "£"
  "\u20ac": "\u20ac", // "€" => "€", already in default alphabet
};

// Emulator will loop back the outgoing SMS if the phone number equals to its
// control port, which is 5554 for the first emulator instance.
const SELF = "5554";

SpecialPowers.setBoolPref("dom.sms.enabled", true);
SpecialPowers.addPermission("sms", true, document);

var manager = window.navigator.mozMobileMessage;
ok(
  manager instanceof MozMobileMessageManager,
  "manager is instance of " + manager.constructor
);

var tasks = {
  // List of test fuctions. Each of them should call |tasks.next()| when
  // completed or |tasks.finish()| to jump to the last one.
  _tasks: [],
  _nextTaskIndex: 0,

  push: function(func) {
    this._tasks.push(func);
  },

  next: function() {
    let index = this._nextTaskIndex++;
    let task = this._tasks[index];
    try {
      task();
    } catch (ex) {
      ok(false, "test task[" + index + "] throws: " + ex);
      // Run last task as clean up if possible.
      if (index != this._tasks.length - 1) {
        this.finish();
      }
    }
  },

  finish: function() {
    this._tasks[this._tasks.length - 1]();
  },

  run: function() {
    this.next();
  },
};

function testStrict7BitEncodingHelper(sent, received) {
  // The log message contains unicode and Marionette seems unable to process
  // it and throws: |UnicodeEncodeError: 'ascii' codec can't encode character
  // u'\xa5' in position 14: ordinal not in range(128)|.
  //
  //log("Testing '" + sent + "' => '" + received + "'");

  let count = 0;
  function done(step) {
    count += step;
    if (count >= 2) {
      window.setTimeout(tasks.next.bind(tasks), 0);
    }
  }

  manager.addEventListener("received", function onReceived(event) {
    event.target.removeEventListener("received", onReceived);

    let message = event.message;
    is(message.body, received, "received message.body");

    done(1);
  });

  let request = manager.send(SELF, sent);
  request.addEventListener("success", function onRequestSuccess(event) {
    let message = event.target.result;
    is(message.body, sent, "sent message.body");

    done(1);
  });
  request.addEventListener("error", function onRequestError(event) {
    ok(false, "Can't send message out!!!");
    done(2);
  });
}

// Bug 877141 - If you send several spaces together in a sms, the other
//              dipositive receives a "*" for each space.
//
// This function is called twice, with strict 7bit encoding enabled or
// disabled.  Expect the same result in both sent and received text and with
// either strict 7bit encoding enabled or disabled.
function testBug877141() {
  log("Testing bug 877141");
  let sent = "1 2     3";
  testStrict7BitEncodingHelper(sent, sent);
}

tasks.push(function() {
  log("Testing with dom.sms.strict7BitEncoding enabled");
  SpecialPowers.setBoolPref("dom.sms.strict7BitEncoding", true);
  tasks.next();
});

// Test for combined string.
tasks.push(function() {
  let sent = "",
    received = "";
  for (let c in GSM_SMS_STRICT_7BIT_CHARMAP) {
    sent += c;
    received += GSM_SMS_STRICT_7BIT_CHARMAP[c];
  }
  testStrict7BitEncodingHelper(sent, received);
});

// When strict7BitEncoding is enabled, we should replace characters that
// can't be encoded with GSM 7-Bit alphabets with '*'.
tasks.push(function() {
  // "Happy New Year" in Chinese.
  let sent = "\u65b0\u5e74\u5feb\u6a02",
    received = "****";
  testStrict7BitEncodingHelper(sent, received);
});

tasks.push(testBug877141);

tasks.push(function() {
  log("Testing with dom.sms.strict7BitEncoding disabled");
  SpecialPowers.setBoolPref("dom.sms.strict7BitEncoding", false);
  tasks.next();
});

// Test for combined string.
tasks.push(function() {
  let sent = "";
  for (let c in GSM_SMS_STRICT_7BIT_CHARMAP) {
    sent += c;
  }
  testStrict7BitEncodingHelper(sent, sent);
});

tasks.push(function() {
  // "Happy New Year" in Chinese.
  let sent = "\u65b0\u5e74\u5feb\u6a02";
  testStrict7BitEncodingHelper(sent, sent);
});

tasks.push(testBug877141);

// WARNING: All tasks should be pushed before this!!!
tasks.push(function cleanUp() {
  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  SpecialPowers.clearUserPref("dom.sms.strict7BitEncoding");

  finish();
});

tasks.run();
