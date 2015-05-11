# SOAAP tools for Chromium

This repository contains tools for applying SOAAP to Chromium.
To use them, you need to:

1. Build [SOAAP](https://github.com/CTSRD-SOAAP/soaap.git) and its
   custom versions of
   [LLVM](https://github.com/CTSRD-SOAAP/llvm.git)
   and
   [Clang](https://github.com/CTSRD-SOAAP/clang.git).

1. Check out the version of Chromium you're interested in:

    ```shell
    $ git submodule init
    $ git submodule update --reference . v42    # or v32, or...
    ```

1. Use the `run-gyp` command to run Gyp (Chromium's meta-build tool)
   with appropriate arguments for FreeBSD and patch the resulting
   Ninja file with SOAAP-specific build targets:

    ```shell
    $ cd v42
    $ LLVM_PREFIX=/llvm/build/path SOAAP_PREFIX=/SOAAP/build/path ../run-gyp
    ```

1. Run `ninja` to build Chromium and perform the SOAAP analysis:
    ```shell
    $ ninja -C out/Release chrome soaap
    ```

You can then inspect the output file `out/Release/chrome.soaap.json`
for unsandboxed past-vulnerability warnings, call traces, etc.

The default `chrome` target builds Chrome in the normal fashion but also
generates an LLVM IR file containing a linked version of Chrome.
To convert this IR file into a native executable, run:

```shell
$ ninja -C out/Release chrome.bc.exe
```
