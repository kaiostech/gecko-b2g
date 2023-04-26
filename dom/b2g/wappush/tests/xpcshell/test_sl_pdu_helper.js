/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var SL = {};
Services.scriptloader.loadSubScript(
  "resource://gre/modules/SlPduHelper.jsm",
  SL
);
SL.debug = info;

/**
 * SL in plain text
 */
add_test(function test_sl_parse_plain_text() {
  let contentType = "";
  let data = {};

  contentType = "text/vnd.wap.sl";
  data.array = new Uint8Array([
    0x3c,
    0x3f,
    0x78,
    0x6d,
    0x6c,
    0x20,
    0x76,
    0x65,
    0x72,
    0x73,
    0x69,
    0x6f,
    0x6e,
    0x3d,
    0x27,
    0x31,
    0x2e,
    0x30,
    0x27,
    0x3f,
    0x3e,
    0x0a,
    0x3c,
    0x73,
    0x6c,
    0x20,
    0x68,
    0x72,
    0x65,
    0x66,
    0x3d,
    0x27,
    0x68,
    0x74,
    0x74,
    0x70,
    0x3a,
    0x2f,
    0x2f,
    0x77,
    0x77,
    0x77,
    0x2e,
    0x6f,
    0x72,
    0x65,
    0x69,
    0x6c,
    0x6c,
    0x79,
    0x2e,
    0x63,
    0x6f,
    0x6d,
    0x27,
    0x2f,
    0x3e,
  ]);
  data.offset = 0;
  let result = "<?xml version='1.0'?>\n<sl href='http://www.oreilly.com'/>";
  let msg = SL.PduHelper.parse(data, contentType);
  do_check_eq(msg.content, result);

  run_next_test();
});

/**
 * SL compressed by WBXML
 */
add_test(function test_sl_parse_wbxml() {
  let msg = {};
  let contentType = "";
  let data = {};

  contentType = "application/vnd.wap.slc";
  data.array = new Uint8Array([
    0x03,
    0x06,
    0x6a,
    0x00,
    0x85,
    0x0a,
    0x03,
    0x6f,
    0x72,
    0x65,
    0x69,
    0x6c,
    0x6c,
    0x79,
    0x00,
    0x85,
    0x01,
  ]);
  data.offset = 0;
  let result = '<sl href="http://www.oreilly.com/"/>';
  msg = SL.PduHelper.parse(data, contentType);
  do_check_eq(msg.content, result);

  run_next_test();
});

/**
 * SL compressed by WBXML, with public ID stored in string table
 */
add_test(function test_sl_parse_wbxml_public_id_string_table() {
  let msg = {};
  let contentType = "";
  let data = {};

  contentType = "application/vnd.wap.slc";
  data.array = new Uint8Array([
    0x03,
    0x00,
    0x00,
    0x6a,
    0x1c,
    0x2d,
    0x2f,
    0x2f,
    0x57,
    0x41,
    0x50,
    0x46,
    0x4f,
    0x52,
    0x55,
    0x4d,
    0x2f,
    0x2f,
    0x44,
    0x54,
    0x44,
    0x20,
    0x53,
    0x4c,
    0x20,
    0x31,
    0x2e,
    0x30,
    0x2f,
    0x2f,
    0x45,
    0x4e,
    0x00,
    0x85,
    0x0a,
    0x03,
    0x6f,
    0x72,
    0x65,
    0x69,
    0x6c,
    0x6c,
    0x79,
    0x00,
    0x85,
    0x01,
  ]);
  data.offset = 0;
  let result = '<sl href="http://www.oreilly.com/"/>';
  msg = SL.PduHelper.parse(data, contentType);
  do_check_eq(msg.content, result);

  run_next_test();
});

/**
 * SL compressed by WBXML with string table
 */
add_test(function test_sl_parse_wbxml_with_string_table() {
  let msg = {};
  let contentType = "";
  let data = {};

  contentType = "application/vnd.wap.slc";
  data.array = new Uint8Array([
    0x03,
    0x06,
    0x6a,
    0x08,
    0x6f,
    0x72,
    0x65,
    0x69,
    0x6c,
    0x6c,
    0x79,
    0x00,
    0x85,
    0x0a,
    0x83,
    0x00,
    0x85,
    0x01,
  ]);
  data.offset = 0;
  let result = '<sl href="http://www.oreilly.com/"/>';
  msg = SL.PduHelper.parse(data, contentType);
  do_check_eq(msg.content, result);

  run_next_test();
});
