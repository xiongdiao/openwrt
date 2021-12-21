#
# Copyright (C) 2008-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=helloworld
PKG_RELEASE:=3
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define KernelPackage/helloworld
  SUBMENU:=Other modules
  TITLE:=HelloWorld
  FILES:=$(PKG_BUILD_DIR)/helloworld.ko
  KCONFIG:=
endef

define KernelPackage/helloworld/description
 Kernel module for register helloworld.
endef

MAKE_OPTS:= \
	$(KERNEL_MAKE_FLAGS) \
	M="$(PKG_BUILD_DIR)"

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

$(eval $(call KernelPackage,helloworld))
