include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=max2871
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define KernelPackage/max2871
  SUBMENU:=Other modules
  TITLE:=MAX2871 PLL/VCO SPI driver
  DEPENDS:=@TARGET_bcm27xx +kmod-spi-bcm2835
  FILES:=$(PKG_BUILD_DIR)/max2871.ko
  # Chưa AUTOLOAD: đang phát triển, nạp tay bằng insmod
endef

define KernelPackage/max2871/description
  SPI driver for the Maxim MAX2871 23.5-6000 MHz synthesizer,
  plus the device tree overlay max2871.dtbo (dtoverlay=max2871).
endef

# ./src/* được package.mk tự copy vào $(PKG_BUILD_DIR); ./files thì không
define Build/Compile
	$(KERNEL_MAKE) M="$(PKG_BUILD_DIR)" modules
	$(LINUX_DIR)/scripts/dtc/dtc -@ -I dts -O dtb \
		-o $(PKG_BUILD_DIR)/max2871.dtbo ./files/max2871-overlay.dts
	# Image bcm27xx chép $(DTS_DIR)/overlays/*.dtbo vào phân vùng boot (vfat)
	# (target/linux/bcm27xx/image/Makefile, Build/boot-common)
	$(CP) $(PKG_BUILD_DIR)/max2871.dtbo \
		$(LINUX_DIR)/arch/$(LINUX_KARCH)/boot/dts/overlays/
endef

# /boot là phân vùng vfat, được mount sẵn khi opkg chạy trên Pi
define KernelPackage/max2871/install
	$(INSTALL_DIR) $(1)/boot/overlays
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/max2871.dtbo $(1)/boot/overlays/
endef

$(eval $(call KernelPackage,max2871))
