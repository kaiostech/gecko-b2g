// ensure that the directory we are writing into is empty
try {
  var f = Services.dirsvc.get("TmpD", Ci.nsIFile);
  f.appendRelativePath("device-storage-testing");
  f.remove(true);
} catch (e) {}

// eslint-disable-next-line no-undef
sendAsyncMessage("directory-removed", {});
