#
# MAX2871 23.5-6000 MHz PLL/VCO - out-of-tree SPI driver for Raspberry Pi
#
# Cấu trúc theo package/kernel/gpio-button-hotplug:
#   src/    : mã nguồn + Kbuild Makefile (package-defaults.mk tự copy vào PKG_BUILD_DIR)
#   files/  : overlay device tree (phải tham chiếu tường minh)
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=max2871
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

# Mục trong menuconfig: Kernel modules -> SPI Support -> kmod-max2871
define KernelPackage/max2871
  SUBMENU:=SPI Support
  TITLE:=MAX2871 PLL/VCO SPI driver
  DEPENDS:=@TARGET_bcm27xx +kmod-spi-bcm2835
  FILES:=$(PKG_BUILD_DIR)/max2871.ko
  # Load at boot: kmodloader reads /etc/modules.d/max2871
  AUTOLOAD:=$(call AutoProbe,max2871)
endef

define KernelPackage/max2871/description
 SPI driver for the Maxim MAX2871 23.5-6000 MHz fractional/integer-N
 synthesizer/VCO. Also builds the device tree overlay max2871.dtbo
 (enable with dtoverlay=max2871 in /boot/config.txt).
endef

define Build/Compile
	$(KERNEL_MAKE) M="$(PKG_BUILD_DIR)" modules
	$(LINUX_DIR)/scripts/dtc/dtc -@ -I dts -O dtb \
		-o $(PKG_BUILD_DIR)/max2871.dtbo ./files/max2871-overlay.dts
	# Image bcm27xx chép $(DTS_DIR)/overlays/*.dtbo vào phân vùng boot (vfat)
	# (target/linux/bcm27xx/image/Makefile, Build/boot-common)
	$(CP) $(PKG_BUILD_DIR)/max2871.dtbo \
		$(LINUX_DIR)/arch/$(LINUX_KARCH)/boot/dts/overlays/
endef

# Dùng khi cài bằng opkg lúc chạy (/boot là phân vùng vfat đã mount)
define KernelPackage/max2871/install
	$(INSTALL_DIR) $(1)/boot/overlays
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/max2871.dtbo $(1)/boot/overlays/
endef

$(eval $(call KernelPackage,max2871))
