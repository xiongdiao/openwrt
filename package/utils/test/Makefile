#
# Copyright (C) 2009-2010 Jo-Philipp Wich <xm@subsignal.org>
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk


PKG_NAME:=test_hello
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

PKG_USE_MIPS16:=0


include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/nls.mk

define Package/test_hello
	SECTION:=utils
	CATEGORY:=Base system
	TITLE:=Userspace  test_hello
	#DEPENDS:=@TARGET_ar71xx 
endef

define Package/test_hello/description
This package contains an utility to manipulate NVRAM on Broadcom based devices.
It works on bcm47xx (Linux 2.6) without using the kernel api.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef
define Build/Configure
endef

define Package/test_hello/install
	$(INSTALL_DIR) $(1)/$(PKG_BIN_DIR) #没有此句不会编译
endef

$(eval $(call BuildPackage,test_hello))
