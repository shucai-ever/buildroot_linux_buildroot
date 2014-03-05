#############################################################
#
# Amlogic Trustzone client application
#
#############################################################
AML_BDK_CA_VERSION = 0.5
AML_BDK_CA_SITE = $(TOPDIR)/package/aml_bdk_ca/amlogic_ca_bdk_v0.5
AML_BDK_CA_SITE_METHOD = local

define AML_BDK_CA_BUILD_CMDS
	cp -arf $(AML_BDK_CA_SITE)/lib/*.so $(@D)/lib/
	$(MAKE) -j1 -C $(@D) all
endef

define AML_BDK_CA_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/lib/*.so $(TARGET_DIR)/usr/lib/
	rm $(@D)/out/apps/*.o
	$(INSTALL) -D -m 0755 $(@D)/out/apps/* $(TARGET_DIR)/usr/bin/
endef

define AML_BDK_CA_CLEAN_CMDS
	$(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D) clean
endef

$(eval $(generic-package))
