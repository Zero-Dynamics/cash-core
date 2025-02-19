packages := boost openssl libevent gmp backtrace
native_packages := native_ccache

qt_packages = qrencode zlib

qt_x86_64_linux_packages := qt expat dbus libxcb xcb_proto libXau xproto freetype fontconfig libxkbcommon libxcb_util libxcb_util_render libxcb_util_keysyms libxcb_util_image libxcb_util_wm libX11 xextproto libXext xtrans
qt_i686_linux_packages := $(qt_x86_64_linux_packages)

qrencode_linux_packages = qrencode
qrencode_android_packages = qrencode
qrencode_darwin_packages = qrencode
qrencode_mingw32_packages = qrencode

qt_android_packages = qt
qt_darwin_packages = qt
qt_mingw32_packages = qt

wallet_packages = bdb

zmq_packages = zeromq

upnp_packages = miniupnpc

multiprocess_packages = libmultiprocess capnp
multiprocess_native_packages = native_libmultiprocess native_capnp

darwin_native_packages = native_biplist native_ds_store native_mac_alias

$(host_arch)_$(host_os)_native_packages += native_b2

ifneq ($(build_os),darwin)
darwin_native_packages += native_cctools native_libtapi native_libdmg-hfsplus

ifeq ($(strip $(FORCE_USE_SYSTEM_CLANG)),)
darwin_native_packages+= native_clang
endif

endif
