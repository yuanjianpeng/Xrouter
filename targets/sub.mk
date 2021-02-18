
target/rootfs/clean:
	$(E)
	$(HQ)rm -fr $(ROOTFS)

target/fit:
	$(E)
	$(H)cd $(KERNEL_BUILD) && $(Q)$(KERNEL_SRC)/scripts/gen_initramfs_list.sh \
		-o $(OUTPUT)/initramfs.cpio.xz -u 0 -g 0 $(ROOTFS)
	$(HQ)cp $(MODEL_DIR)/fit-image.its $(OUTPUT)/
	$(H)cd $(OUTPUT) && $(Q)mkimage -f fit-image.its $(OUTPUT)/$(MODEL).itb

target/bin:
	$(E)
	$(H)cd $(OUTPUT) && $(MKFW) -c $(MODEL_DIR)/firmware.config \
		-k $(HOST)/xrouter.priv $(MODEL).bin

target: target/fit target/bin;

