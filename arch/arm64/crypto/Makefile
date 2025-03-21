#
# linux/arch/arm64/crypto/Makefile
#
# Copyright (C) 2014 Linaro Ltd <ard.biesheuvel@linaro.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#

obj-$(CONFIG_CRYPTO_SHA1_ARM64_CE) += sha1-ce.o
sha1-ce-y := sha1-ce-glue.o sha1-ce-core.o

obj-$(CONFIG_CRYPTO_SHA2_ARM64_CE) += sha2-ce.o
sha2-ce-y := sha2-ce-glue.o sha2-ce-core.o

obj-$(CONFIG_CRYPTO_GHASH_ARM64_CE) += ghash-ce.o
ghash-ce-y := ghash-ce-glue.o ghash-ce-core.o

obj-$(CONFIG_CRYPTO_CRCT10DIF_ARM64_CE) += crct10dif-ce.o
crct10dif-ce-y := crct10dif-ce-core.o crct10dif-ce-glue.o

obj-$(CONFIG_CRYPTO_CRC32_ARM64_CE) += crc32-ce.o
crc32-ce-y:= crc32-ce-core.o crc32-ce-glue.o

obj-$(CONFIG_CRYPTO_AES_ARM64_CE) += aes-ce-cipher.o
aes-ce-cipher-y := aes-ce-core.o aes-ce-glue.o

ifeq ($(cc-name),gcc)
        CFLAGS_aes-ce-cipher-core.o += $(call cc-ifversion, -lt, 0800, -mcpu=cortex-a73.cortex-a53 -mtune=cortex-a73.cortex-a53)
        CFLAGS_aes-ce-cipher-core.o += $(call cc-ifversion, -ge, 0800, -mcpu=cortex-a75.cortex-a55 -mtune=cortex-a75.cortex-a55)
else
    CFLAGS_aes-ce-cipher-core.o += -mcpu=cortex-a55+crypto -mtune=cortex-a55+crypto
endif

obj-$(CONFIG_CRYPTO_AES_ARM64_CE_CCM) += aes-ce-ccm.o
aes-ce-ccm-y := aes-ce-ccm-glue.o aes-ce-ccm-core.o

obj-$(CONFIG_CRYPTO_AES_ARM64_CE_BLK) += aes-ce-blk.o
aes-ce-blk-y := aes-glue-ce.o aes-ce.o

obj-$(CONFIG_CRYPTO_AES_ARM64_NEON_BLK) += aes-neon-blk.o
aes-neon-blk-y := aes-glue-neon.o aes-neon.o

obj-$(CONFIG_CRYPTO_CHACHA20_NEON) += chacha20-neon.o
chacha20-neon-y := chacha20-neon-core.o chacha20-neon-glue.o

obj-$(CONFIG_CRYPTO_SHA256_ARM64) += sha256-arm64.o
sha256-arm64-y := sha256-glue.o sha256-core.o

obj-$(CONFIG_CRYPTO_SHA512_ARM64) += sha512-arm64.o
sha512-arm64-y := sha512-glue.o sha512-core.o

AFLAGS_aes-ce.o		:= -DINTERLEAVE=4
AFLAGS_aes-neon.o	:= -DINTERLEAVE=4

CFLAGS_aes-glue-ce.o	:= -DUSE_V8_CRYPTO_EXTENSIONS

obj-$(CONFIG_CRYPTO_CRC32_ARM64) += crc32-arm64.o

# FIXME : error: switch -mcpu=armv8-a conflicts with -march=armv8.2-a switch
CFLAGS_crc32-arm64.o	+= $(call cc-ifversion, -lt, 0700, -mcpu=generic+crc)

$(obj)/aes-glue-%.o: $(src)/aes-glue.c FORCE
	$(call if_changed_rule,cc_o_c)

quiet_cmd_perlasm = PERLASM $@
      cmd_perlasm = $(PERL) $(<) void $(@)

$(src)/sha256-core.S_shipped: $(src)/sha512-armv8.pl
	$(call cmd,perlasm)

$(src)/sha512-core.S_shipped: $(src)/sha512-armv8.pl
	$(call cmd,perlasm)

.PRECIOUS: $(obj)/sha256-core.S $(obj)/sha512-core.S
