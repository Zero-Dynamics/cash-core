package=openssl
$(package)_version=3.4.1
$(package)_download_path=https://github.com/openssl/openssl/releases/download/$(package)-$($(package)_version)
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=002a2d6b30b58bf4bea46c43bdd96365aaf8daa6c428782aa4feee06da197df3

define $(package)_set_vars
$(package)_config_env=AR="$($(package)_ar)" ARFLAGS=$($(package)_arflags) RANLIB="$($(package)_ranlib)" CC="$($(package)_cc)"
$(package)_config_env+=WINDRES="x86_64-w64-mingw32-windres"
$(package)_config_env+=RC="x86_64-w64-mingw32-windres"
$(package)_config_env_arm_android=ANDROID_NDK_HOME="$(host_prefix)/native" PATH="$(host_prefix)/native/bin" CC=clang AR=ar RANLIB=ranlib
$(package)_config_env_aarch64_android=ANDROID_NDK_HOME="$(host_prefix)/native" PATH="$(host_prefix)/native/bin" CC=clang AR=ar RANLIB=ranlib
$(package)_build_env_arm_android=ANDROID_NDK_HOME="$(host_prefix)/native"
$(package)_build_env_aarch64_android=ANDROID_NDK_HOME="$(host_prefix)/native"
$(package)_config_env_x86_64_darwin+=LD="x86_64-apple-darwin-ld"
$(package)_config_opts=--prefix=$(host_prefix) --openssldir=$(host_prefix)/etc/openssl
$(package)_config_opts+=enable-asm
$(package)_config_opts+=no-capieng
$(package)_config_opts+=no-dso
$(package)_config_opts+=no-dtls1
$(package)_config_opts+=no-gost
$(package)_config_opts+=no-heartbeats
$(package)_config_opts+=no-md2
$(package)_config_opts+=no-rc5
$(package)_config_opts+=no-rdrand
$(package)_config_opts+=no-rfc3779
$(package)_config_opts+=no-sctp
$(package)_config_opts+=no-shared
$(package)_config_opts+=no-ssl-trace
$(package)_config_opts+=no-ssl2
$(package)_config_opts+=no-ssl3
$(package)_config_opts+=no-unit-test
$(package)_config_opts+=no-weak-ssl-ciphers
$(package)_config_opts+=no-zlib
$(package)_config_opts+=no-zlib-dynamic
$(package)_config_opts+=$($(package)_cflags) $($(package)_cppflags)
$(package)_config_opts_linux=-fPIC -Wa,--noexecstack
$(package)_config_opts_freebsd=-fPIC -Wa,--noexecstack
$(package)_config_opts_x86_64_linux=linux-x86_64
$(package)_config_opts_i686_linux=linux-generic32
$(package)_config_opts_arm_linux=linux-generic32
$(package)_config_opts_aarch64_linux=linux-generic64
$(package)_config_opts_arm_android=--static android-arm
$(package)_config_opts_aarch64_android=--static android-arm64
$(package)_config_opts_riscv64_linux=linux-generic64
$(package)_config_opts_mipsel_linux=linux-generic32
$(package)_config_opts_mips_linux=linux-generic32
$(package)_config_opts_powerpc_linux=linux-generic32
$(package)_config_opts_x86_64_darwin=darwin64-x86_64-cc
$(package)_config_opts_x86_64_mingw32=mingw64
$(package)_config_opts_i686_mingw32=mingw
$(package)_config_opts_x86_64_freebsd=BSD-x86_64
endef

define $(package)_preprocess_cmds
  sed -i.old 's|"engines", "apps", "test", "util", "tools", "fuzz"|"engines", "tools"|' Configure && \
  sed -i -e 's|cflags --sysroot.*",|cflags",|' Configurations/15-android.conf
endef

define $(package)_config_cmds
  ./Configure $($(package)_config_opts)
endef

define $(package)_build_cmds
  $(MAKE) -j1 build_libs libcrypto.pc libssl.pc openssl.pc
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) -j1 install_sw
endef

define $(package)_postprocess_cmds
  rm -rf share bin etc
endef
