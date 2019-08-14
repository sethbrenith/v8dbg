# v8dbg
This project is a WinDbg extension for the V8 engine. It uses the DataModel as
much as possible (see [DataModel Manager]) via the native interfaces using the
[C++/WinRT COM] APIs.

The source in the root directory is a generic starting point for implementing a
WinDbg extension. The V8-specific implementation (under `./src`) then implements
the two methods declared near the top of `dbgext.h` to create and destroy the
extension instance.

## Hard-coded prerequisites

This is not yet ready for easy consumption. It includes the following hard-coded
paths, which you will need to update in the code (main.cpp and runtests.bat) to
point to the appropriate location on your machine.

* A V8 output directory, where you have built at least the targets `d8` and
  `v8_debug_helper`, with gn arg `is_component_build = true`. Currently
  `f:\repos\ana\v8\out\debug_x64`.
* The path for this extension's built output. Currently
  `d:\repos\v8dbg\x64\v8dbg.dll`.
* The path for your installation of WinDbgX. Currently
  `%LOCALAPPDATA%\DBG\UI\Slow.20190520.1\amd64`.
* The path for a JavaScript file to run when executing the test executable.
  Currently `d:\scripts\wrapper.js`. This can be any valid JavaScript program.
  The test executable will say it fails regardless, but will also print the
  result of the `dx` command on the value returned by the script (the return
  value of the script's last statement). An empty file is a valid script.

## Building

1. Open a `Native x64 Developer Tools` command prompt installed by VS 2019.
   (Note: the developer VS prompt defaults to x86 and that causes build errors
   if you try to use it.)
2. Create a `.\x64` directory under the project and CD into it.
3. To create the build files run: `cmake -G Ninja ../`
4. To build, in the same directory run: `ninja` (or, from the root directory,
   run `cmake --build ./x64`).

The resulting `v8dbg.dll` and symbols should be generated in the build directory.

### Release builds

The above will create a debug build by default. To build a release build, create
a release directory to use and change the CMake command to:

`cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo ../`

## Testing

Use the `runtests.bat` script in the root directory (after building) to run the
console app that exercises the extension. Launch with `runtests.bat dbg` to run
the test executable in an instance of WinDbgx.

As the version of dbgeng.dll that comes with Windows is a system DLL, it is
found first by default, but the system version does not allow loading of extensions.
Thus the script has to copy the extension and test executable to the WinDbgx
location to load the correct dbgeng.dll and dbgmodel.dll files.

The local path to WinDbgx in the first line of `runtests.bat` may need updating.

## Debugging the extension

To debug the extension, launch a WinDbgx instance to debug with an active target, e.g.

`windbgx \src\github\v8\out\x64.debug\d8.exe -e "console.log('hello');"`

or

`windbgx \src\github\v8\out\x64.debug\d8.exe c:\temp\test.js`

The WinDbgx process itself does not host the extensions, but a helper process.
Attach another instance of WinDbgx to the `enghost.exe` helper process, e.g.

`windbgx -pn enghost.exe`

Set a breakpoint in this second session for when the extension initializes, e.g.

`bm v8dbg!DebugExtensionInitialize`

..and/or whenever a function of interest is invoked, e.g.

 - `bp v8dbg!CurrIsolateAlias::Call` for the invocation of `@$curisolate()`
 - `bp v8dbg!GetHeapObject` for the interpretation of V8 objects.

Load the extension in the target debugger (the first WinDbg session), which
should trigger the breakpoint.

`.load "C:\\src\\github\\v8dbg\\x64\\v8dbg.dll"`

Note: For D8, the below is a good breakpoint to set just before any script is run:

`bp d8_exe!v8::Shell::ExecuteString`

..or the below for once the V8 engine is entered (for component builds):

`bp v8!v8::Script::Run`

Then trigger the extension code of interest via something like `dx source` or
`dx @$curisolate()`.

[DataModel Manager]: https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/data-model-cpp-overview
[C++/WinRT COM]: https://docs.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/consume-com
