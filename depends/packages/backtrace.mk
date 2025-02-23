package=backtrace
$(package)_version=0034e33946824057b48c5e686a3aefc761b37384
$(package)_download_path=https://github.com/ianlancetaylor/libbacktrace/archive
$(package)_download_file=$($(package)_version).tar.gz
$(package)_file_name=$(package)_$($(package)_version).tar.gz
$(package)_sha256_hash=8a5d15df5f4a6c20d9defd4192498615e5ca1b81b872b2433200eb8c75122dbf

define $(package)_set_vars
$(package)_config_opts=--disable-shared --enable-host-shared --prefix=$(host_prefix)
endef

define $(package)_config_cmds
  cp -f $(BASEDIR)/config.guess $(BASEDIR)/config.sub . && \
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm lib/*.la
endef
