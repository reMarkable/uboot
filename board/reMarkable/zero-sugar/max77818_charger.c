#include "max77818_charger.h"
#include "max77818.h"
#include "max77818_battery.h"

#include <asm/arch/sys_proto.h>
#include <asm/arch/mx7-pins.h>
#include <asm/gpio.h>
#include <asm/mach-imx/iomux-v3.h>
#include <dm/uclass.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <i2c.h>

#define MAX77818_REG_DETAILS_0						0xB3
#define MAX77818_DETAILS_0__BAT_DET_MASK				0x01
#define MAX77818_DETAILS_0__BAT_DET_SHIFT				0x00
#define MAX77818_DETAILS_0__BAT_PRESENT					0x00
#define MAX77818_DETAILS_0__BAT_NOT_PRESENT				0x01
#define MAX77818_DETAILS_0__WCIN_DETAILS_MASK				0x18
#define MAX77818_DETAILS_0__WCIN_DETAILS_SHIFT				0x03
#define MAX77818_DETAILS_0__INV_WCIN_BELOW_WCINUVLO			0x00
#define MAX77818_DETAILS_0__INV_WCIN_ABOVE_WCINUVLO			0x01
#define MAX77818_DETAILS_0__INV_WCIN_ABOVE_WCINOVLO			0x02
#define MAX77818_DETAILS_0__OK_WCIN_BELOW_WCINOVLO			0x03
#define MAX77818_DETAILS_0__CHGIN_DETAILS_MASK				0x60
#define MAX77818_DETAILS_0__CHGIN_DETAILS_SHIFT				0x05
#define MAX77818_DETAILS_0__INV_CHGIN_BELOW_CHGINUVLO			0x00
#define MAX77818_DETAILS_0__INV_CHGIN_ABOVE_CHGINUVLO			0x01
#define MAX77818_DETAILS_0__INV_CHGIN_ABOVE_CHGINOVLO			0x02
#define MAX77818_DETAILS_0__OK_CHGIN_BELOW_CHGINOVLO			0x03

#define MAX77818_REG_DETAILS_1						0xB4

#define MAX77818_REG_DETAILS_2						0xB5

#define MAX77818_REG_CHGPROT						0xBD
#define MAX77818_CHGPROT__MASK						0x0C
#define MAX77818_CHGPROT__UNLOCK					0x0C
#define MAX77818_CHGPROT__LOCK						0x00

#define MAX77818_REG_CHG_CNFG_0						0xB7
#define MAX77818_CHG_CNFG_0__MODE__MASK					0x0F
#define MAX77818_CHG_CNFG_0__MODE__OTG_BOOST_BUCK_ON			0x0E
#define MAX77818_CHG_CNFG_0__MODE__CHARGER_BUCK_O			0x05

#define MAX77818_REG_CHG_CNFG_02					0xB9
#define MAX77818_CHG_CNFG_02__CHARGE_CC__MASK				0x3F
#define MAX77818_CHG_CNFG_02__CHARGE_CC__FAST_CHARGE_1P5_A		0x1E
#define MAX77818_CHG_CNFG_02__CHARGE_CC__FAST_CHARGE_2P8_A		0x38

#define MAX77818_REG_CHG_CNFG_04					0xBB
#define MAX77818_CHG_CNFG_04__CHG_CV_PRM__MASK				0x3F
#define MAX77818_CHG_CNFG_04__CHG_CV_PRM__4V3				0x1A
#define MAX77818_CHG_CNFG_04__MINVSYS__MASK				0xC0
#define MAX77818_CHG_CNFG_04__MINVSYS__3P4V_VSYS_MIN			0x00

#define MAX77818_REG_CHG_CNFG_09					0xC0
#define MAX77818_CHG_CNFG_09__CHGIN_ILIM__MASK				0x7f
#define MAX77818_CHG_CNFG_09__CHGIN_ILIM_500_MA				0x0f
#define MAX77818_CHG_CNFG_09__CHGIN_ILIM_1P5_A				0x2D
#define MAX77818_CHG_CNFG_09__CHGIN_ILIM_2P8_A				0x54

#define MAX77818_REG_CHG_CNFG_10					0xC1
#define MAX77818_CHG_CNFG_10__WCIN_ILIM_500_MA				0x19
#define MAX77818_CHG_CNFG_10__WCIN_ILIM__MASK				0x3F
#define MAX77818_CHG_CNFG_10__WCIN_ILIM__1P26_A				0x3F

static struct udevice *chDev = NULL;

static int max77818_init_charger_device(bool leave_fgcc_disabled, bool *restore_state)
{
	int ret;
	bool fgcc_restore_state;

	struct udevice *bus = max77818_get_bus();
	if (!bus) {
		ret = max77818_init_i2c_bus();
		if (ret) {
			printf("%s: Unable to complete charger device initialization",
			       __func__);
			return ret;
		}
	}

	/* Turn off FGCC in order to do required charger config, if enabled */
	printf("Disabling FGCC mode in order to do minimal charger config\n");
	ret = max77818_set_fgcc_state(false, &fgcc_restore_state);
	if (ret) {
		printf("%s: Failed to disable FGCC mode: %d\n",
		       __func__,
		       ret);
		return ret;
	}

	/* Slight delay to let the charger device get back online */
	mdelay(100);

	ret = dm_i2c_probe(bus, MAX77818_CHARGER_I2C_ADDR, 0, &chDev);
	if (ret) {
		printf("%s: Can't find device id=0x%x, on bus %d\n",
		__func__, MAX77818_CHARGER_I2C_ADDR, MAX77818_I2C_BUS);
		return -ENODEV;
	}

	if (!leave_fgcc_disabled) {
		ret = max77818_restore_fgcc(fgcc_restore_state);
		if (ret) {
			printf("%s: Failed to restore FGCC mode: %d\n",
			       __func__,
			       ret);
			return ret;
		}
	}

	*restore_state = fgcc_restore_state;
	return 0;
}

static int max77818_set_reg_lock(struct udevice *dev, bool locked)
{
	u8 regVal;

	regVal = (locked ? MAX77818_CHGPROT__LOCK : MAX77818_CHGPROT__UNLOCK);
	return max77818_i2c_reg_write8(dev,
				       MAX77818_REG_CHGPROT,
				       MAX77818_CHGPROT__MASK, regVal);
}

static int max77818_get_details_1(struct udevice *dev)
{
	u8 regVal;
	int ret;

	ret = dm_i2c_read(dev, MAX77818_REG_DETAILS_1, &regVal, 1);
	if (ret) {
		return ret;
	}

	printf("Read DETAILS_1: 0x%02X\n", regVal);

	return 0;
}

static int max77818_get_details_2(struct udevice *dev)
{
	u8 regVal;
	int ret;

	ret = dm_i2c_read(dev, MAX77818_REG_DETAILS_2, &regVal, 1);
	if (ret) {
		return ret;
	}

	printf("Read DETAILS_2: 0x%02X\n", regVal);

	return 0;
}

int max77818_get_battery_status(struct udevice *dev)
{
	u8 regVal;
	int ret;

	ret = dm_i2c_read(dev, MAX77818_REG_DETAILS_0, &regVal, 1);
	if (ret) {
		printf("Failed to read DETAIL_0 register !\n");
		return ret;
	}

	regVal &= MAX77818_DETAILS_0__BAT_DET_MASK;
	regVal >>= MAX77818_DETAILS_0__BAT_DET_SHIFT;

	switch(regVal) {
	case MAX77818_DETAILS_0__BAT_PRESENT:
		printf("Battery is present\n");
		break;
	case MAX77818_DETAILS_0__BAT_NOT_PRESENT:
		printf("Battery is NOT present\n");
		break;
	default:
		printf("Unknown battery detection state: 0x%02X\n", regVal);
	}

	return 0;
}

int max77818_get_wcin_details(struct udevice *dev)
{
	u8 regVal;
	int ret;

	ret = dm_i2c_read(dev, MAX77818_REG_DETAILS_0, &regVal, 1);
	if (ret) {
		printf("Failed to read DETAIL_0 register !\n");
		return ret;
	}

	regVal &= MAX77818_DETAILS_0__WCIN_DETAILS_MASK;
	regVal >>= MAX77818_DETAILS_0__WCIN_DETAILS_SHIFT;

	switch(regVal) {
	case MAX77818_DETAILS_0__INV_WCIN_BELOW_WCINUVLO:
		printf("INVALID VWCIN:\nWCIN < VWCIN_UVLO\n");
		break;

	case MAX77818_DETAILS_0__INV_WCIN_ABOVE_WCINUVLO:
		printf("INVALID VWCIN: VWCIN < VMBAT + VWCIN2SYS and "
		       "VWCIN > VWCIN_UVLO\n");
		break;

	case MAX77818_DETAILS_0__INV_WCIN_ABOVE_WCINOVLO:
		printf("INVALID VWCIN:\nVWCIN is invalid. VWCIN>VWCIN_OVLO\n");
		break;

	case MAX77818_DETAILS_0__OK_WCIN_BELOW_WCINOVLO:
		printf("VWCIN OK:\n"
		       "VWCIN > VWCIN_UVLO, VWCIN > VMBAT + VWCIN2SYS, "
		       "VWCIN < VWCIN_OVLO\n");
		break;

	default:
		printf("Unknown winc status: 0x%02X\n", regVal);
	}

	return 0;
}

int max77818_get_chgin_details(struct udevice *dev)
{
	u8 regVal;
	int ret;

	ret = dm_i2c_read(dev, MAX77818_REG_DETAILS_0, &regVal, 1);
	if (ret) {
		printf("Failed to read DETAIL_0 register !\n");
		return ret;
	}

	regVal &= (u8)MAX77818_DETAILS_0__CHGIN_DETAILS_MASK;
	printf("CHING DETAIL VALUE: 0x%02X\n", regVal);

	regVal = regVal >> (u8)MAX77818_DETAILS_0__CHGIN_DETAILS_SHIFT;
	printf("CHING DETAIL VALUE: 0x%02X\n", regVal);
	switch(regVal) {
	case MAX77818_DETAILS_0__INV_CHGIN_BELOW_CHGINUVLO:
		printf("INVALID VBUS:\nCHGIN < VCHGIN_UVLO\n");
		break;

	case MAX77818_DETAILS_0__INV_CHGIN_ABOVE_CHGINUVLO:
		printf("INVALID VBUS: VCHGIN < VMBAT + VCHGIN2SYS and "
		       "VCHGIN > VCHGIN_UVLO");
		break;

	case MAX77818_DETAILS_0__INV_CHGIN_ABOVE_CHGINOVLO:
		printf("INVALID VBUS:\n"
		       "VCHGIN is invalid. "
		       "VCHGIN>VCHGIN_OVLO\n");
		break;

	case MAX77818_DETAILS_0__OK_CHGIN_BELOW_CHGINOVLO:
		printf("VBUS OK:\n"
		       "VCHGIN > VCHGIN_UVLO, VCHGIN > VMBAT + VCHGIN2SYS, "
		       "VCHGIN < VCHGIN_OVLO\n");
		break;

	default:
		printf("Unknown winc status: 0x%02X\n", regVal);
	}

	return 0;
}

int max77818_set_otg_pwr(struct udevice *dev, bool otg_on)
{
	u8 regVal;

	dev = (dev ? dev : chDev);
	if (!dev)
		return -1;

	regVal = (otg_on ?
		  MAX77818_CHG_CNFG_0__MODE__OTG_BOOST_BUCK_ON :
		  MAX77818_CHG_CNFG_0__MODE__CHARGER_BUCK_O);

	return max77818_i2c_reg_write8(dev, MAX77818_REG_CHG_CNFG_0,
				       MAX77818_CHG_CNFG_0__MODE__MASK, regVal);
}

int max77818_set_fast_charge_current(struct udevice *dev,
				     enum fast_charge_current fsc)
{
	int ret;
	uint fc_config_value;

	dev = (dev ? dev : chDev);
	if (!dev)
		return -1;

	ret = max77818_set_reg_lock(dev, false);
	if (ret)
		return ret;

	switch(fsc) {
	case FASTCHARGE_1P5_A:
		fc_config_value =
			MAX77818_CHG_CNFG_02__CHARGE_CC__FAST_CHARGE_1P5_A;
		break;
	case FASTCHARGE_2P8_A:
		fc_config_value =
			MAX77818_CHG_CNFG_02__CHARGE_CC__FAST_CHARGE_2P8_A;
		break;
	default:
		/* Invalid value, just set default 1.5A */
		fc_config_value =
			MAX77818_CHG_CNFG_02__CHARGE_CC__FAST_CHARGE_1P5_A;
	}

	ret = max77818_i2c_reg_write8(dev,
				      MAX77818_REG_CHG_CNFG_02,
				      MAX77818_CHG_CNFG_02__CHARGE_CC__MASK,
				      fc_config_value);
	if (ret)
		return ret;

	return max77818_set_reg_lock(dev, true);
}

int max77818_set_charge_termination_voltage(struct udevice *dev)
{
	int ret;

	dev = (dev ? dev : chDev);
	if (!dev)
		return -1;

	ret = max77818_set_reg_lock(dev, false);
	if (ret)
		return ret;

	ret = max77818_i2c_reg_write8(dev,
				      MAX77818_REG_CHG_CNFG_04,
				      MAX77818_CHG_CNFG_04__CHG_CV_PRM__MASK,
				      MAX77818_CHG_CNFG_04__CHG_CV_PRM__4V3);
	if (ret)
		return ret;

	ret = max77818_i2c_reg_write8(dev,
				      MAX77818_REG_CHG_CNFG_04,
				      MAX77818_CHG_CNFG_04__MINVSYS__MASK,
				      MAX77818_CHG_CNFG_04__MINVSYS__3P4V_VSYS_MIN);
	if (ret)
		return ret;

	return max77818_set_reg_lock(dev, true);
}

int max77818_set_pogo_input_current_limit(struct udevice *dev,
					  enum pogo_ilim ilim)
{
	uint ilim_config_value;

	dev = (dev ? dev : chDev);
	if (!dev)
		return -1;

	switch(ilim) {
	case ILIM_500_MA:
		ilim_config_value = MAX77818_CHG_CNFG_09__CHGIN_ILIM_500_MA;
		break;
	case ILIM_1P5_A:
		ilim_config_value = MAX77818_CHG_CNFG_09__CHGIN_ILIM_1P5_A;
		break;
	case ILIM_2P8_A:
		ilim_config_value = MAX77818_CHG_CNFG_09__CHGIN_ILIM_2P8_A;
		break;
	default:
		/* Invalid value, just set default 500 mA */
		ilim_config_value = MAX77818_CHG_CNFG_09__CHGIN_ILIM_500_MA;
	}

	return max77818_i2c_reg_write8(dev,
				       MAX77818_REG_CHG_CNFG_09,
				       MAX77818_CHG_CNFG_09__CHGIN_ILIM__MASK,
				       ilim_config_value);
}

int max77818_set_usbc_input_current_limit(struct udevice *dev)
{
	dev = (dev ? dev : chDev);
	if (!dev)
		return -1;

	return max77818_i2c_reg_write8(dev,
				       MAX77818_REG_CHG_CNFG_10,
				       MAX77818_CHG_CNFG_10__WCIN_ILIM__MASK,
				       MAX77818_CHG_CNFG_10__WCIN_ILIM_500_MA);
}

int max77818_set_minimal_charger_config(void)
{
	int ret;
	bool fgcc_restore_state;

	if (!chDev) {
		ret = 	max77818_init_charger_device(true, &fgcc_restore_state);
		if (ret) {
			return ret;
		}
	}

	printf("Trying to set fast charge current: 1.5A\n");
	ret = max77818_set_fast_charge_current(NULL, FASTCHARGE_1P5_A);
	if (ret != 0)
		printf("%s Failed to set fast charger current\n",
		       __func__);

	printf("Trying to set pogo input current limit: 500 mA\n");
	ret = max77818_set_pogo_input_current_limit(NULL, ILIM_500_MA);
	if (ret != 0)
		printf("%s: Failed to set pogo input current limit\n",
		       __func__);

	printf("Trying to set USB-C input current limit: 500 mA\n");
	ret = max77818_set_usbc_input_current_limit(NULL);
	if (ret != 0)
		printf("%s: Failed to set USB-C input current limit\n",
		       __func__);

	printf("Trying to set normal charge mode (turn off OTG mode if set)\n");
	ret = max77818_set_otg_pwr(NULL, false);
	if (ret != 0)
		printf("%s: Failed to set normal charge mode\n",
		       __func__);

	ret = max77818_restore_fgcc(fgcc_restore_state);
	if (ret) {
		printf("%s: Failed to restore FGCC: %d\n",
		       __func__,
		       ret);
		return ret;
	}


	return 0;
}

static int zs_do_get_battery_charge_status(cmd_tbl_t *cmdtp,
					   int flag,
					   int argc,
					   char * const argv[])
{
	int ret;
	bool fgcc_restore_state;

	if (!chDev) {
		ret = max77818_init_charger_device(true, &fgcc_restore_state);
		if(ret) {
			printf("%s: Unable to get battery charge status\n",
			       __func__);
			return ret;
		}
	}

	ret = max77818_get_battery_status(chDev);
	if (ret) {
		printf("Failed to get battery detection status: %d\n", ret);
		return ret;
	}

	ret = max77818_get_wcin_details(chDev);
	if (ret) {
		printf("Failed to get wcin (USB-C) status: %d\n", ret);
		return ret;
	}

	ret = max77818_get_chgin_details(chDev);
	if (ret) {
		printf("Failed to get chgin (POGO) status: %d\n", ret);
		return ret;
	}

	ret = max77818_get_details_1(chDev);
	if (ret) {
		printf("Failed to get DETAILS_1 status: 0x%02X\n", ret);
		return ret;
	}

	ret = max77818_get_details_2(chDev);
	if (ret) {
		printf("Failed to get DETAILS_2 status: 0x%02X\n", ret);
		return ret;
	}

	ret = max77818_restore_fgcc(fgcc_restore_state);
	if (ret) {
		printf("%s: Failed to restore FGCC: %d\n",
		       __func__,
		       ret);
		return ret;
	}

	return 0;
}
U_BOOT_CMD(
	max77818_get_battery_charge_status, 1, 1, zs_do_get_battery_charge_status,
	"Get battery and charge voltage status",
	"Read battery detection status, USB-C voltage status and POGO input voltage status"
);

static int zs_do_set_otg_pwr(cmd_tbl_t *cmdtp,
			     int flag,
			     int argc,
			     char * const argv[])
{
	int ret;
	bool fgcc_restore_state;

	if (argc != 2) {
		printf("Usage: set_otg_power <on | off>\n");
		return -1;
	}

	if ((strcmp(argv[1], "on") != 0) && (strcmp(argv[1], "off") != 0)) {
		printf("Usage: set_otg_power <on | off>\n");
		return -1;
	}

	if (!chDev) {
		ret = max77818_init_charger_device(true, &fgcc_restore_state);
		if(ret) {
			printf("%s: Unable to set OTG power\n",
			       __func__);
			return ret;
		}
	}

	if (strcmp(argv[1], "on") == 0) {
		ret = max77818_set_otg_pwr(chDev, true);
		if (ret) {
			printf("Failed to turn OTG power on: %d\n", ret);
			return ret;
		}
	}
	else {
		ret = max77818_set_otg_pwr(chDev, false);
		if (ret) {
			printf("Failed to turn OTG power off: %d\n", ret);
			return ret;
		}
	}

	ret = max77818_restore_fgcc(fgcc_restore_state);
	if (ret) {
		printf("%s: Failed to restore FGCC: %d\n",
		       __func__,
		       ret);
		return ret;
	}

	return 0;
}
U_BOOT_CMD(
	max77818_set_otg_pwr,	2,	1,	zs_do_set_otg_pwr,
	"Turn on/off OTG power",
	"Turn off charging, and enable OTG power output to connected device"
);

static int zs_do_set_fastcharge_current_1P5_A(cmd_tbl_t *cmdtp,
					      int flag,
					      int argc,
					      char * const argv[])
{
	int ret;
	bool fgcc_restore_state;

	if (argc != 1) {
		printf("Usage: set_fastcharge_current_2P8_A\n");
		return -1;
	}

	if (!chDev) {
		ret = max77818_init_charger_device(true, &fgcc_restore_state);
		if(ret) {
			printf("%s: Unable to set fastcharge current (1.5A)\n",
			       __func__);
			return ret;
		}
	}

	ret = max77818_set_fast_charge_current(chDev, FASTCHARGE_1P5_A);
	if (ret) {
		printf("Failed to set fast charge current: %d\n", ret);
		return ret;
	}

	ret = max77818_restore_fgcc(fgcc_restore_state);
	if (ret) {
		printf("%s: Failed to restore FGCC: %d\n",
		       __func__,
		       ret);
		return ret;
	}

	return 0;
}
U_BOOT_CMD(
	max77818_set_fast_charge_current_1P5_A, 1, 1, zs_do_set_fastcharge_current_1P5_A,
	"Set fastcharge current (1.5A)",
	"Set fastcharge current to 1.5A for the pogo pin charge input"
);

static int zs_do_set_fastcharge_current_2P8_A(cmd_tbl_t *cmdtp,
					      int flag,
					      int argc,
					      char * const argv[])
{
	int ret;
	bool fgcc_restore_state;

	if (argc != 1) {
		printf("Usage: set_fastcharge_current_2P8_A\n");
		return -1;
	}

	if (!chDev) {
		ret = max77818_init_charger_device(true, &fgcc_restore_state);
		if(ret) {
			printf("%s: Unable to set fastcharge current (2.8A)\n",
			       __func__);
			return ret;
		}
	}

	ret = max77818_set_fast_charge_current(chDev, FASTCHARGE_2P8_A);
	if (ret) {
		printf("Failed to set fast charge current: %d\n", ret);
		return ret;
	}

	ret = max77818_restore_fgcc(fgcc_restore_state);
	if (ret) {
		printf("%s: Failed to restore FGCC: %d\n",
		       __func__,
		       ret);
		return ret;
	}
	return 0;
}
U_BOOT_CMD(
	max77818_set_fast_charge_current_2P8_A, 1, 1, zs_do_set_fastcharge_current_2P8_A,
	"Set fastcharge current (2.8A)",
	"Set fastcharge current to 2.8A for the pogo pin charge input"
);

static int zs_do_set_charge_termination_voltage(cmd_tbl_t *cmdtp,
						int flag,
						int argc,
						char * const argv[])
{
	int ret;
	bool fgcc_restore_state;

	if (argc != 1) {
		printf("Usage: set_charge_termination_voltage\n");
		return -1;
	}

	if (!chDev) {
		ret = max77818_init_charger_device(true, &fgcc_restore_state);
		if(ret) {
			printf("%s: Unable to set fastcharge current (2.8A)\n",
			       __func__);
			return ret;
		}
	}

	ret = max77818_set_charge_termination_voltage(chDev);
	if (ret) {
		printf("Failed to set charge termination voltage: %d\n", ret);
		return ret;
	}

	ret = max77818_restore_fgcc(fgcc_restore_state);
	if (ret) {
		printf("%s: Failed to restore FGCC: %d\n",
		       __func__,
		       ret);
		return ret;
	}
	return 0;
}
U_BOOT_CMD(
	max77818_set_charge_termination_voltage, 1, 1, zs_do_set_charge_termination_voltage,
	"Set charge termination voltage (3.4V)",
	"Set charge termination voltage to 3.4V"
);

static int zs_do_set_pogo_input_current_limit_1P5_A(cmd_tbl_t *cmdtp,
						    int flag,
						    int argc,
						    char * const argv[])
{
	int ret;
	bool fgcc_restore_state;

	if (argc != 1) {
		printf("Usage: set_pogo_input_current_limit\n");
		return -1;
	}

	if (!chDev) {
		ret = max77818_init_charger_device(true, &fgcc_restore_state);
		if(ret) {
			printf("%s: Unable to set fastcharge current (2.8A)\n",
			       __func__);
			return ret;
		}
	}

	ret = max77818_set_pogo_input_current_limit(chDev, ILIM_1P5_A);
	if (ret) {
		printf("Failed to set pogo input current limit: %d\n", ret);
		return ret;
	}

	ret = max77818_restore_fgcc(fgcc_restore_state);
	if (ret) {
		printf("%s: Failed to restore FGCC: %d\n",
		       __func__,
		       ret);
		return ret;
	}
	return 0;
}
U_BOOT_CMD(
	max77818_set_pogo_input_current_limit_1P5_A, 1, 1, zs_do_set_pogo_input_current_limit_1P5_A,
	"Set max pogo input current (1.5A)",
	"Set max charge input current limit to 1.5A for the pogo pin charge input"
);

static int zs_do_set_pogo_input_current_limit_2P8_A(cmd_tbl_t *cmdtp,
						    int flag,
						    int argc,
						    char * const argv[])
{
	int ret;
	bool fgcc_restore_state;

	if (argc != 1) {
		printf("Usage: set_pogo_input_current_limit\n");
		return -1;
	}

	if (!chDev) {
		ret = max77818_init_charger_device(true, &fgcc_restore_state);
		if(ret) {
			printf("%s: Unable to set fastcharge current (2.8A)\n",
			       __func__);
			return ret;
		}
	}

	ret = max77818_set_pogo_input_current_limit(chDev, ILIM_2P8_A);
	if (ret) {
		printf("Failed to set pogo input current limit: %d\n", ret);
		return ret;
	}

	ret = max77818_restore_fgcc(fgcc_restore_state);
	if (ret) {
		printf("%s: Failed to restore FGCC: %d\n",
		       __func__,
		       ret);
		return ret;
	}
	return 0;
}
U_BOOT_CMD(
	max77818_set_pogo_input_current_limit_2P8_A, 1, 1, zs_do_set_pogo_input_current_limit_2P8_A,
	"Set max pogo input current (2.8A)",
	"Set max charge input current limit to 2.8A for the pogo pin charge input"
);

static int zs_do_set_usbc_input_current_limit(cmd_tbl_t *cmdtp,
					      int flag,
					      int argc,
					      char * const argv[])
{
	int ret;
	bool fgcc_restore_state;

	if (argc != 1) {
		printf("Usage: set_usbc_input_current_limit\n");
		return -1;
	}

	if (!chDev) {
		ret = max77818_init_charger_device(true, &fgcc_restore_state);
		if(ret) {
			printf("%s: Unable to set fastcharge current (2.8A)\n",
			       __func__);
			return ret;
		}
	}

	ret = max77818_set_usbc_input_current_limit(chDev);
	if (ret) {
		printf("Failed to turn OTG power off: %d\n", ret);
		return ret;
	}

	ret = max77818_restore_fgcc(fgcc_restore_state);
	if (ret) {
		printf("%s: Failed to restore FGCC: %d\n",
		       __func__,
		       ret);
		return ret;
	}
	return 0;
}
U_BOOT_CMD(
	max77818_set_usbc_input_current_limit, 1, 1, zs_do_set_usbc_input_current_limit,
	"Set max USB-C input current (500 mA)",
	"Set max charge input current limit to 500 mA for the USB-C charge input"
);

static int read_gpio(unsigned gpio)
{
	int ret;

	ret = gpio_request(gpio, "gpio");
	if (ret) {
		printf("gpio_request failed for gpio: %u\n", gpio);
		return ret;
	}

	ret = gpio_direction_input(gpio);
	if (ret) {
		printf("gpio_direction_input failed for gpio: %u\n", gpio);
		return ret;
	}

	return gpio_get_value(gpio);
}

static iomux_v3_cfg_t const chargestat_pads[] = {
	MX7D_PAD_SAI2_TX_SYNC__GPIO6_IO19 | MUX_PAD_CTRL(PAD_CTL_PUS_PU100KOHM),
	MX7D_PAD_SAI2_TX_BCLK__GPIO6_IO20 | MUX_PAD_CTRL(PAD_CTL_PUS_PU100KOHM),
};

bool max77818_is_charging(void)
{
	int ret;
	unsigned gpio_chgin = IMX_GPIO_NR(6, 19);
	unsigned gpio_wcin = IMX_GPIO_NR(6, 20);
	bool stat_chgin, stat_wcin;

	imx_iomux_v3_setup_multiple_pads(chargestat_pads, ARRAY_SIZE(chargestat_pads));

	ret = read_gpio(gpio_chgin);
	stat_chgin = (ret == 0);

	ret = read_gpio(gpio_wcin);
	stat_wcin = (ret == 0);

	return (stat_chgin || stat_wcin);
}
