# Android

Scaffolding for `ndk-build`. **Not built in this repository's CI** — there is no Android NDK
in the build environment, and bionic's headers are not something a C compiler can conjure. The
files are here so the port is a command, not a research project.

## What is verified, and what is not

| Claim | Verified |
| --- | --- |
| The core cross-compiles for **aarch64** and **armv7** (musl) | yes — `make cross` |
| The core cross-compiles for **riscv64**, **x86_64-windows**, **aarch64-macos** | yes — `make cross` |
| The core compiles against **bionic** (Android's libc) | **no** — needs the NDK |
| The game links raylib for Android and runs | **no** — needs the NDK and raylib's Android build |

ARM is what Android devices run, so the instruction selection, float behaviour and struct
layout are already exercised; what remains untested is bionic and the raylib glue.

## Build it

```bash
# 1. raylib for Android (its own build, once)
git clone https://github.com/raysan5/raylib
export NDK_MODULE_PATH=$PWD/raylib

# 2. the game
cd android && ndk-build
```

Output lands in `android/libs/<abi>/libgk.so` and `android/obj/`.
