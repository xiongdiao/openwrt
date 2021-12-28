# All rights reserved.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=mt7663
PKG_VERSION:=gerrit
#PKG_SOURCE:=MT7663_LinuxAP_6.0.2.0_20190503-e3e040.tar.bz2

include $(INCLUDE_DIR)/package.mk

define KernelPackage/mt7663
  CATEGORY:=MTK Properties
  SUBMENU:=Drivers
  MENU:=1
  TITLE:=MT7663 wifi driver
  DEPENDS:=+mt7663-scripts +wifi-l1profile +mtk-base-files +wifi_services
endef

define KernelPackage/mt7663-in
  HIDDEN:=1
  CATEGORY:=MTK Properties
  SUBMENU:=Drivers
  TITLE:=MT7663e BUILD_IN
  DEPENDS:=+mt7663-scripts
  FILES:=
  AUTOLOAD:=
  KCONFIG:= \
    CONFIG_WIFI_MT7663E=y \
    CONFIG_MT_MAC=y \
    CONFIG_MT_WIFI=y \
    CONFIG_MT_AP_SUPPORT=y \
    CONFIG_WIFI_MODE_AP=y
endef

define KernelPackage/mt7663-ko
  HIDDEN:=1
  CATEGORY:=MTK Properties
  SUBMENU:=Drivers
  TITLE:=MT7663e BUILD_KO
  DEPENDS:=+mt7663-scripts +wifi_services
  #FILES:=$(LINUX_DIR)/drivers/net/wireless/mtk/mt7663/mt_wifi_ap/mt7663.ko
  FILES:=$(PKG_BUILD_DIR)/mt_wifi_ap/mt7663.ko
  AUTOLOAD:=$(call AutoProbe,mt7663)
  KCONFIG:= \
    CONFIG_WIFI_MT7663E=y \
    CONFIG_MT_MAC=y \
    CONFIG_MT_WIFI=m \
    CONFIG_MT_AP_SUPPORT=m \
    CONFIG_WIFI_MODE_AP=m
endef

define KernelPackage/mt7663/config
	if PACKAGE_kmod-mt7663
	source "$(SOURCE)/config.in"
	endif
endef

define Build/Prepare
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

NOSTDINC_FLAGS := \
	$(KERNEL_NOSTDINC_FLAGS) \
	-I$(PKG_BUILD_DIR)/mt_wifi/include\

PKG_MAKE_FLAGS += CONFIG_WIFI_MT7663E=y 
PKG_MAKE_FLAGS += CONFIG_CHIP_MT7663E=y 
PKG_MAKE_FLAGS += CONFIG_SUPPORT_OPENWRT=y
PKG_MAKE_FLAGS += CONFIG_MT_AP_SUPPORT=m
PKG_MAKE_FLAGS += CONFIG_ATE_SUPPORT=y
PKG_MAKE_FLAGS += CONFIG_APCLI_SUPPORT=y
PKG_MAKE_FLAGS += CONFIG_DOT11_VHT_AC=y
PKG_MAKE_FLAGS += CONFIG_MBO_SUPPORT=y
PKG_MAKE_FLAGS += CONFIG_INTERWORKING=y
PKG_MAKE_FLAGS += CONFIG_PASSPOINT_R2=y
PKG_MAKE_FLAGS += CONFIG_TXBF_SUPPORT=y
PKG_MAKE_FLAGS += CONFIG_OWE_SUPPORT=y
PKG_MAKE_FLAGS += CONFIG_WPA3_SUPPORT=y
PKG_MAKE_FLAGS += CONFIG_MAC_REPEATER_SUPPORT=y
PKG_MAKE_FLAGS += CONFIG_WSC_INCLUDED=y
PKG_MAKE_FLAGS += CONFIG_WSC_V2_SUPPORT=y
PKG_MAKE_FLAGS += CONFIG_MBSS_SUPPORT=y
PKG_MAKE_FLAGS += CONFIG_MT_DFS_SUPPORT=y
PKG_MAKE_FLAGS += CONFIG_OCE_SUPPORT=y
PKG_MAKE_FLAGS += CONFIG_LED_CONTROL_SUPPORT=y
PKG_MAKE_FLAGS += CONFIG_BACKGROUND_SCAN_SUPPORT=y
PKG_MAKE_FLAGS += CONFIG_UAPSD=y
PKG_MAKE_FLAGS += CONFIG_DOT11K_RRM_SUPPORT=y
PKG_MAKE_FLAGS += CONFIG_MT_MAC=y




define Build/Compile
	+$(MAKE) $(PKG_JOBS) -C "$(LINUX_DIR)" \
		$(KERNEL_MAKE_FLAGS) \
		$(PKG_MAKE_FLAGS) \
		M="$(PKG_BUILD_DIR)" \
		NOSTDINC_FLAGS="$(NOSTDINC_FLAGS)" \
		modules
endef

define Package/mt7663-scripts
  CATEGORY:=MTK Properties
  SUBMENU:=Drivers
  TITLE:=mt7663 scripts
  DEPENDS:=+mtk-base-files
  HIDDEN:=1
endef

define Package/mt7663-scripts/install
	$(INSTALL_DIR) $(1)/lib/wifi/
	$(INSTALL_DIR) $(1)/etc/wireless/mt7663/
	-if [ "$$(CONFIG_WIFI_SCRIPT_LUA)" = "y" ]; then \
	$(INSTALL_BIN) ./files/mt7663.lua $(1)/lib/wifi/; \
	else \
	$(INSTALL_BIN) ./files/mt7663.sh $(1)/lib/wifi/; \
	fi
	echo $(PKG_VERSION) > $(1)/etc/wireless/mt7663/version

	if [ "$$(CONFIG_KCONFIG_FIRST_IF_MT7663E)" = "y" ]; then \
		$(INSTALL_BIN) ./files/mt7663*.bin $(1)/etc/wireless/mt7663/; \
		$(INSTALL_BIN) ./files/mt7663-sku.dat $(1)/etc/wireless/mt7663/; \
		$(INSTALL_BIN) ./files/mt7663-sku-bf.dat $(1)/etc/wireless/mt7663/; \
		if [ "$$(CONFIG_PACKAGE_libmapd)" = "y" ]; then \
			$(INSTALL_BIN) ./files/mt7663.1.map.dat $(1)/etc/wireless/mt7663/mt7663.1.dat; \
		else \
			$(INSTALL_BIN) ./files/mt7663.1.dat $(1)/etc/wireless/mt7663/; \
		fi \
	fi

	if [ "$$(CONFIG_KCONFIG_SECOND_IF_MT7663E)" = "y" ]; then \
		$(INSTALL_BIN) ./files/mt7663*.bin $(1)/etc/wireless/mt7663/; \
		$(INSTALL_BIN) ./files/mt7663-sku.dat $(1)/etc/wireless/mt7663/; \
		$(INSTALL_BIN) ./files/mt7663-sku-bf.dat $(1)/etc/wireless/mt7663/; \
		if [ "$$(CONFIG_PACKAGE_libmapd)" = "y" ]; then \
			$(INSTALL_BIN) ./files/mt7663.2.map.dat $(1)/etc/wireless/mt7663/mt7663.2.dat; \
		else \
			$(INSTALL_BIN) ./files/mt7663.2.dat $(1)/etc/wireless/mt7663/; \
		fi \
	fi
endef

define Package/mt7663-scripts/conffiles
/etc/wireless/mt7663/
endef

$(eval $(call KernelPackage,mt7663))
$(eval $(call KernelPackage,mt7663-ko))
$(eval $(call KernelPackage,mt7663-in))
$(eval $(call BuildPackage,mt7663-scripts))
