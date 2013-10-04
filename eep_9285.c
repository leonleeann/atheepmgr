/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "edump.h"
#include "eep_9285.h"

struct eep_9285_priv {
	struct ar9285_eeprom eep;
};

static int eep_9285_get_ver(struct eep_9285_priv *emp)
{
	return ((emp->eep.baseEepHeader.version >> 12) & 0xF);
}

static int eep_9285_get_rev(struct eep_9285_priv *emp)
{
	return ((emp->eep.baseEepHeader.version) & 0xFFF);
}

static bool eep_9285_fill(struct edump *edump)
{
#define EEP_9285_SIZE (sizeof(struct ar9285_eeprom) / sizeof(uint16_t))

	struct eep_9285_priv *emp = edump->eepmap_priv;
	uint16_t *eep_data = (uint16_t *)&emp->eep;
	int addr, eep_start_loc = 0;

	eep_start_loc = 64;

	for (addr = 0; addr < EEP_9285_SIZE; addr++) {
		if (!EEP_READ(addr + eep_start_loc, eep_data)) {
			fprintf(stderr, "Unable to read eeprom region\n");
			return false;
		}
		eep_data++;
	}

	return true;

#undef EEP_9285_SIZE
}

static bool eep_9285_check(struct edump *edump)
{
#define EEP_9285_SIZE (sizeof(struct ar9285_eeprom) / sizeof(uint16_t))

	struct eep_9285_priv *emp = edump->eepmap_priv;
	struct ar9285_eeprom *eep = &emp->eep;
	uint16_t *eepdata, temp, magic, magic2;
	uint32_t sum = 0, el;
	bool need_swap = false;
	int i, addr;

	if (!EEP_READ(AR5416_EEPROM_MAGIC_OFFSET, &magic)) {
		fprintf(stderr, "Reading Magic # failed\n");
		return false;
	}

	if (magic != AR5416_EEPROM_MAGIC) {
		magic2 = bswap_16(magic);

		if (magic2 == AR5416_EEPROM_MAGIC) {
			need_swap = true;
			eepdata = (uint16_t *)eep;

			for (addr = 0; addr < EEP_9285_SIZE; addr++) {
				temp = bswap_16(*eepdata);
				*eepdata = temp;
				eepdata++;
			}
		} else {
			fprintf(stderr, "Invalid EEPROM Magic, endianness mismatch.\n");
			return false;
		}
	}

	if (need_swap)
		el = bswap_16(eep->baseEepHeader.length);
	else
		el = eep->baseEepHeader.length;

	if (el > sizeof(struct ar9285_eeprom))
		el = sizeof(struct ar9285_eeprom) / sizeof(uint16_t);
	else
		el = el / sizeof(uint16_t);

	eepdata = (uint16_t *)eep;

	for (i = 0; i < el; i++)
		sum ^= *eepdata++;

	if (need_swap) {
		uint32_t integer;
		uint16_t word;

		printf("EEPROM Endianness is not native.. Changing\n");

		word = bswap_16(eep->baseEepHeader.length);
		eep->baseEepHeader.length = word;

		word = bswap_16(eep->baseEepHeader.checksum);
		eep->baseEepHeader.checksum = word;

		word = bswap_16(eep->baseEepHeader.version);
		eep->baseEepHeader.version = word;

		word = bswap_16(eep->baseEepHeader.regDmn[0]);
		eep->baseEepHeader.regDmn[0] = word;

		word = bswap_16(eep->baseEepHeader.regDmn[1]);
		eep->baseEepHeader.regDmn[1] = word;

		word = bswap_16(eep->baseEepHeader.rfSilent);
		eep->baseEepHeader.rfSilent = word;

		word = bswap_16(eep->baseEepHeader.blueToothOptions);
		eep->baseEepHeader.blueToothOptions = word;

		word = bswap_16(eep->baseEepHeader.deviceCap);
		eep->baseEepHeader.deviceCap = word;

		integer = bswap_32(eep->modalHeader.antCtrlCommon);
		eep->modalHeader.antCtrlCommon = integer;

		for (i = 0; i < AR9285_MAX_CHAINS; i++) {
			integer = bswap_32(eep->modalHeader.antCtrlChain[i]);
			eep->modalHeader.antCtrlChain[i] = integer;
		}

		for (i = 0; i < AR_EEPROM_MODAL_SPURS; i++) {
			word = bswap_16(eep->modalHeader.spurChans[i].spurChan);
			eep->modalHeader.spurChans[i].spurChan = word;
		}
	}

	if (sum != 0xffff || eep_9285_get_ver(emp) != AR5416_EEP_VER ||
	    eep_9285_get_rev(emp) < AR5416_EEP_NO_BACK_VER) {
		fprintf(stderr, "Bad EEPROM checksum 0x%x or revision 0x%04x\n",
			sum, eep_9285_get_ver(emp));
		return false;
	}

	return true;

#undef EEP_9285_SIZE
}

static void eep_9285_dump_base_header(struct edump *edump)
{
	struct eep_9285_priv *emp = edump->eepmap_priv;
	struct ar9285_eeprom *eep = &emp->eep;
	struct ar9285_base_eep_hdr *pBase = &eep->baseEepHeader;
	uint16_t i;

	printf("\n----------------------\n");
	printf("| EEPROM Base Header |\n");
	printf("----------------------\n\n");

	printf("%-30s : %2d\n", "Major Version",
	       pBase->version >> 12);
	printf("%-30s : %2d\n", "Minor Version",
	       pBase->version & 0xFFF);
	printf("%-30s : 0x%04X\n", "Checksum",
	       pBase->checksum);
	printf("%-30s : 0x%04X\n", "Length",
	       pBase->length);
	printf("%-30s : 0x%04X\n", "RegDomain1",
	       pBase->regDmn[0]);
	printf("%-30s : 0x%04X\n", "RegDomain2",
	       pBase->regDmn[1]);
	printf("%-30s : %02X:%02X:%02X:%02X:%02X:%02X\n",
	       "MacAddress",
	       pBase->macAddr[0], pBase->macAddr[1], pBase->macAddr[2],
	       pBase->macAddr[3], pBase->macAddr[4], pBase->macAddr[5]);
	printf("%-30s : 0x%04X\n",
	       "TX Mask", pBase->txMask);
	printf("%-30s : 0x%04X\n",
	       "RX Mask", pBase->rxMask);
	printf("%-30s : %d\n",
	       "OpFlags(5GHz)",
	       !!(pBase->opCapFlags & AR5416_OPFLAGS_11A));
	printf("%-30s : %d\n",
	       "OpFlags(2GHz)",
	       !!(pBase->opCapFlags & AR5416_OPFLAGS_11G));
	printf("%-30s : %d\n",
	       "OpFlags(Disable 2GHz HT20)",
	       !!(pBase->opCapFlags & AR5416_OPFLAGS_N_2G_HT20));
	printf("%-30s : %d\n",
	       "OpFlags(Disable 2GHz HT40)",
	       !!(pBase->opCapFlags & AR5416_OPFLAGS_N_2G_HT40));
	printf("%-30s : %d\n",
	       "OpFlags(Disable 5Ghz HT20)",
	       !!(pBase->opCapFlags & AR5416_OPFLAGS_N_5G_HT20));
	printf("%-30s : %d\n",
	       "OpFlags(Disable 5Ghz HT40)",
	       !!(pBase->opCapFlags & AR5416_OPFLAGS_N_5G_HT40));
	printf("%-30s : %d\n",
	       "Big Endian",
	       !!(pBase->eepMisc & AR5416_EEPMISC_BIG_ENDIAN));
	printf("%-30s : %d\n",
	       "Cal Bin Major Ver",
	       (pBase->binBuildNumber >> 24) & 0xFF);
	printf("%-30s : %d\n",
	       "Cal Bin Minor Ver",
	       (pBase->binBuildNumber >> 16) & 0xFF);
	printf("%-30s : %d\n",
	       "Cal Bin Build",
	       (pBase->binBuildNumber >> 8) & 0xFF);

	if (eep_9285_get_rev(emp) >= AR5416_EEP_MINOR_VER_3) {
		printf("%-30s : %s\n",
		       "Device Type",
		       sDeviceType[(pBase->deviceType & 0x7)]);
	}

	printf("\nCustomer Data in hex:\n");
	for (i = 0; i < 64; i++) {
		printf("%02X ", eep->custData[i]);
		if ((i % 16) == 15)
			printf("\n");
	}
}

static void eep_9285_dump_modal_header(struct edump *edump)
{
#define PR(_token, _p, _val_fmt, _val)			\
	do {						\
		printf("%-23s %-2s", (_token), ":");	\
		printf("%s%"_val_fmt, _p, (_val));	\
		printf("\n");				\
	} while(0)

	struct eep_9285_priv *emp = edump->eepmap_priv;
	struct ar9285_eeprom *eep = &emp->eep;
	struct ar9285_modal_eep_hdr *pModal = &eep->modalHeader;

	printf("\n\n-----------------------\n");
	printf("| EEPROM Modal Header |\n");
	printf("-----------------------\n\n");

	PR("Ant Chain 0", "0x", "X", pModal->antCtrlChain[0]);
	PR("Antenna Common", "0x", "X", pModal->antCtrlCommon);
	PR("Antenna Gain Chain 0", "", "d", pModal->antennaGainCh[0]);
	PR("Switch Settling", "", "d", pModal->switchSettling);
	PR("TxRxAttenation Chain 0", "", "d", pModal->txRxAttenCh[0]);
	PR("RxTxMargin Chain 0", "", "d", pModal->rxTxMarginCh[0]);
	PR("ADC Desired Size", "", "d", pModal->adcDesiredSize);
	PR("PGA Desired Size", "", "d", pModal->pgaDesiredSize);
	PR("XLNA Gain Chain 0", "", "d", pModal->xlnaGainCh[0]);
	PR("TxEndToXpaOff", "", "d", pModal->txEndToXpaOff);
	PR("TxEndToRxOn", "", "d", pModal->txEndToRxOn);
	PR("TxFrameToXpaOn", "", "d", pModal->txFrameToXpaOn);
	PR("Thresh 62", "", "d", pModal->thresh62);
	PR("NF Thresh Chain 0", "", "d", pModal->noiseFloorThreshCh[0]);
	PR("XPD Gain", "", "d", pModal->xpdGain);
	PR("XPD", "", "d", pModal->xpd);
	PR("IQ Cal I Chain 0", "", "d", pModal->iqCalICh[0]);
	PR("IQ Cal Q Chain 0", "", "d", pModal->iqCalQCh[0]);
	PR("PD Gain Overlap", "", "d", pModal->pdGainOverlap);
	PR("Output Bias CCK", "", "d", pModal->ob_0);
	PR("Output Bias BPSK", "", "d", pModal->ob_1);
	PR("Driver 1 Bias CCK", "", "d", pModal->db1_0);
	PR("Driver 1 Bias BPSK", "", "d", pModal->db1_1);
	PR("XPA Bias Level", "", "d", pModal->xpaBiasLvl);
	PR("TX Frame to Data Start", "", "d", pModal->txFrameToDataStart);
	PR("TX Frame to PA On", "", "d", pModal->txFrameToPaOn);
	PR("HT40PowerIncForPDADC", "", "d", pModal->ht40PowerIncForPdadc);
	PR("bsw_atten Chain 0", "", "d", pModal->bswAtten[0]);
	PR("bsw_margin Chain 0", "", "d", pModal->bswMargin[0]);
	PR("Switch Settling [HT40]", "", "d", pModal->swSettleHt40);
	PR("xatten2DB Chain 0", "", "d", pModal->xatten2Db[0]);
	PR("xatten2margin Chain 0", "", "d", pModal->xatten2Margin[0]);
	PR("Driver 2 Bias CCK", "", "d", pModal->db2_0);
	PR("Driver 2 Bias BPSK", "", "d", pModal->db2_1);
	PR("ob_db Version", "", "d", pModal->version);
	PR("Output Bias QPSK", "", "d", pModal->ob_2);
	PR("Output Bias 16QAM", "", "d", pModal->ob_3);
	PR("Output Bias 64QAM", "", "d", pModal->ob_4);
	PR("Ant diversity ctrl 1", "", "d", pModal->antdiv_ctl1);
	PR("Driver 1 Bias QPSK", "", "d", pModal->db1_2);
	PR("Driver 1 Bias 16QAM", "", "d", pModal->db1_3);
	PR("Driver 1 Bias 64QAM", "", "d", pModal->db1_4);
	PR("Ant diversity ctrl 2", "", "d", pModal->antdiv_ctl2);
	PR("Driver 2 Bias QPSK", "", "d", pModal->db2_2);
	PR("Driver 2 Bias 16QAM", "", "d", pModal->db2_3);
	PR("Driver 2 Bias 64QAM", "", "d", pModal->db2_4);
}

static void eep_9285_dump_power_info(struct edump *edump)
{
}

const struct eepmap eepmap_9285 = {
	.name = "9285",
	.desc = "AR9285 chip EEPROM map",
	.priv_data_sz = sizeof(struct eep_9285_priv),
	.fill_eeprom  = eep_9285_fill,
	.check_eeprom = eep_9285_check,
	.dump_base_header = eep_9285_dump_base_header,
	.dump_modal_header = eep_9285_dump_modal_header,
	.dump_power_info = eep_9285_dump_power_info,
};
