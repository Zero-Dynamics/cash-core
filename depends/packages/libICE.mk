package=libICE
$(package)_version=1.2.4
$(package)_download_path=http://xorg.freedesktop.org/releases/individual/lib/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=9aeb845049de475d0b47aac744e59e1a868ccf95ba669d04c91a1e06239b370b
$(package)_dependencies=xtrans xproto

define $(package)_set_vars
  $(package)_config_opts=--disable-static --disable-docs --disable-specs --without-xsltproc
  $(package)_config_opts_linux=--with-pic
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
