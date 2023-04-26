# DeviceStorage API

`DeviceStorage` is mainly picked back from [Bug 1299500](https://bugzilla.mozilla.org/show_bug.cgi?id=1299500), so the interface remains unchanged. The only exception is method `DeviceStorage.enumerate()`, a new return type replaces DOMCursor since DOMCursor has deprecated already. You can find details of the new return type --- `FileIterable` and examples of using DeviceStorage.enumerate() in this document, for details about other methods, please reference [DeviceStorage](https://developer.mozilla.org/en-US/docs/Archive/B2G_OS/API/DeviceStorage).

## FileIterable

`FileIterable` is declared to be asynchronously iterable, for details about iterable protocol and iterator protocol, please check up [Iteration protocols](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Iteration_protocols), which introduces the *synchronous* version of iterable and iterator. MDN page of asynchronous iterable and iterator does not exist yet, however, they are mostly the same, except:
1. `itor.next()` returns a Promise with the result object.
2. Supports `for-await-of` to loop through the iteration.
3. Does not support indexing.

For more details about asynchronously iterable, please reference the W3C spec at [Asynchronously iterable declarations](https://heycam.github.io/webidl/#idl-async-iterable).

### Interfaces
Interface of FileIterable and DeviceStorage.enumerate() are as follow:

```
[Exposed=Window]
interface FileIterable {
  async iterable<File>;
};
```
```
[Throws]
FileIterable enumerate(optional DeviceStorageEnumerationParameters options = {});
[Throws]
FileIterable enumerate(DOMString path, optional DeviceStorageEnumerationParameters options = {});
[Throws]
FileIterable enumerateEditable(optional DeviceStorageEnumerationParameters options = {});
[Throws]
FileIterable enumerateEditable(DOMString path, optional DeviceStorageEnumerationParameters options = {});
```
### Examples

For example, if there are three files **a.txt**, **b.txt**, **c.txt** in default **internal SD card**, and we want to list them out.

**Use itor.next() to loop through the iteration**

```
var sdcard = navigator.b2g.getDeviceStorage('sdcard');
var iterable = sdcard.enumerate();
var files = iterable.values();

var file1 = await files.next();
// file1 is a Promise and will be resolved with Object { done: false, value: File {name: "/sdcard/a.txt", ...} }

var file2 = await files.next();
// file2 is a Promise and will be resolved with Object { done: false, value: File {name: "/sdcard/b.txt", ...} }

var file3 = await files.next();
// file3 is a Promise and will be resolved with Object { done: false, value: File {name: "/sdcard/c.txt", ...} }

var noMoreFile = await files.next();
// noMoreFile is a Promise and will be resolved with Object { done: true, value: Undefined }
```

**Use for-await-of to loop through the iteration**

```
var sdcard = navigator.b2g.getDeviceStorage('sdcard');
var iterable = sdcard.enumerate();
async function printAllFiles() {
    for await (let file of iterable) {
        console.log(file);
    }
}
printAllFiles();

// Will print out the following in console...
// Object { done: false, value: File {name: "/sdcard/a.txt", ...} }
// Object { done: false, value: File {name: "/sdcard/b.txt", ...} }
// Object { done: false, value: File {name: "/sdcard/c.txt", ...} }
```
