APP_ABI := armeabi-v7a arm64-v8a x86 x86_64

# Do not change APP_PLATFORM if we care about Gingerbread (2.3.3) devices!  We must compile against android-10,
# otherwise we may encounter runtime load-library errors from symbols that should have been inlined against older
# Bionic.  e.g., getpagesize()
APP_PLATFORM := android-10

APP_STL := gnustl_static
