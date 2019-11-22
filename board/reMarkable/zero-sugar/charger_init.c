#include "charger_init.h"

#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include <dm/uclass.h>

#include <linux/errno.h>
#include <linux/types.h>

#include <i2c.h>

#define MAX77818_I2C_BUS						1

#define MAX77818_CHARGER_I2C_ADDR					0x69
#define MAX77818_FG_I2C_ADDR						0x36
#define MAX77818_ID_I2C_ADDR						0x66

#define MAX77818_REG_SAFEOUTCTRL					0xC6
#define MAX77818_SAFEOUTCTRL_ENSAFEOUT1					BIT(6)

#define MAX77818_REG_FG_CONFIG						0x1D
#define MAX77818_FG_CONFIG__FGCC_MASK					0x0800
#define MAX77818_FG_CONFIG__FGCC_ENABLED				0x0800
#define MAX77818_FG_CONFIG__FGCC_DISABLED				0x0000

#define MAX77818_REG_DETAILS_0						0xB3
#define MAX77818_DETAILS_0__BAT_DET_MASK				0x01
#define MAX77818_DETAILS_0__BAT_DET_SHIFT				0x00
#define MAX77818_DETAILS_0__BAT_PRECENT					0x00
#define MAX77818_DETAILS_0__BAT_NOT_PRECENT				0x01
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
#define MAX77818_CHG_CNFG_09__CHGIN_ILIM_1P5_A				0x2D
#define MAX77818_CHG_CNFG_09__CHGIN_ILIM_2P8_A				0x54

#define MAX77818_REG_CHG_CNFG_10					0xC1
#define MAX77818_CHG_CNFG_10__WCIN_ILIM__MASK				0x3F
#define MAX77818_CHG_CNFG_10__WCIN_ILIM__1P26_A				0x3F

static struct udevice *bus = NULL, *idDev = NULL, *chDev = NULL, *fgDev = NULL;
static bool fgcc_restore_state = 0;

static int max77818_i2c_reg_read16(struct udevice *dev,
				   u8 addr,
				   u16 *data);

static int max77818_i2c_reg_write16(struct udevice *dev,
				    u8 addr,
				    u16 mask,
				    u16 data)
{
	int ret;
	u8 val[2];
	u16 finalVal;
	u16 maskedVal;
	struct dm_i2c_chip *chip = dev_get_parent_platdata(dev);

	ret = dm_i2c_read(dev, addr, val, 2);
	if (ret) {
		printf("%s: Failed to read from addr 0x%02x/dev 0x%02x: %d\n",
		       __func__,
		       addr,
		       chip->chip_addr,
		       ret);
		return ret;
	} else {
		finalVal = val[0] | ((u16)val[1] << 8);
	}

	maskedVal = data & mask;
	finalVal &= ~mask;
	finalVal |= maskedVal;
	val[0] = finalVal & 0xff;
	val[1] = (finalVal >> 8) & 0xff;

	ret = dm_i2c_write(dev, addr, val, 2);
	if (ret) {
		printf("%s: Failed to write to addr 0x%02x/dev 0x%02x: %d\n",
		       __func__,
		       addr,
		       chip->chip_addr,
		       ret);
	}

	return 0;
}

static int max77818_i2c_reg_read16(struct udevice *dev,
				   u8 addr,
				   u16 *data)
{
	int ret;
	u8 val[2];
	struct dm_i2c_chip *chip = dev_get_parent_platdata(dev);

	ret = dm_i2c_read(fgDev, addr, val, 2);
	if (ret) {
		printf("%s: Failed to read from addr 0x%02x/dev 0x%02x: %d\n",
		       __func__,
		       addr,
		       chip->chip_addr,
		       ret);
		return ret;
	} else {
		*data = val[0] | ((u16)val[1] << 8);
	}

	return 0;
}


static int max77818_i2c_reg_write8(struct udevice *dev,
				  u8 addr,
				  u8 mask,
				  u8 data)
{
	u8 valb;
	int ret;

	if (mask != 0xff) {
		ret = dm_i2c_read(dev, addr, &valb, 1);
		if (ret)
			return ret;

		valb &= ~mask;
		valb |= data;
	} else
		valb = data;

	ret = dm_i2c_write(dev, addr, &valb, 1);
	return ret;
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

int max77818_init_device(void)
{
	int ret;

	ret = uclass_get_device_by_seq(UCLASS_I2C, MAX77818_I2C_BUS, &bus);
	if (ret) {
		printf("%s: Can't find I2C bus %d (expected for MAX77818)\n",
		       __func__, MAX77818_I2C_BUS);
		return -1;
	}

	ret = dm_i2c_probe(bus, MAX77818_ID_I2C_ADDR, 0, &idDev);
	if (ret) {
		printf("%s: Can't find device id=0x%x, on bus %d\n",
		__func__, MAX77818_ID_I2C_ADDR, MAX77818_I2C_BUS);
		return -1;
	}

	ret = dm_i2c_probe(bus, MAX77818_FG_I2C_ADDR, 0, &fgDev);
	if (ret) {
		printf("%s: Can't find device id=0x%x, on bus %d\n",
		__func__, MAX77818_FG_I2C_ADDR, MAX77818_I2C_BUS);
		return -1;
	}

	/* Read current FGCC state to be restored after config */
	printf("%s: Reading current FGCC state to be restored\n",
	       __func__);
	ret = max77818_read_fgcc_state(&fgcc_restore_state);
	if (ret) {
		printf("%s: Unable to read current FGCC state,"
		       "assuming FGCC should be configured\n",
		       __func__);
		fgcc_restore_state = true;
	}

	/* Turn off FGCC in order to do required charger config, if enabled */
	if (fgcc_restore_state) {
		printf("Turning off FGCC in order to do minimal charger config\n");
		ret = max77818_enable_fgcc(false);
		if (ret) {
			printf("%s: Failed to disable FGCC mode: %d\n",
			       __func__,
			       ret);
			return -1;
		}

		mdelay(100);
	}
	else {
		printf("FGCC currently not configured, no need to disable\n");
	}

	ret = dm_i2c_probe(bus, MAX77818_CHARGER_I2C_ADDR, 0, &chDev);
	if (ret) {
		printf("%s: Can't find device id=0x%x, on bus %d\n",
		__func__, MAX77818_CHARGER_I2C_ADDR, MAX77818_I2C_BUS);
	}
	return 0;
}

int max77818_read_fgcc_state(bool *state)
{
	int ret;
	u16 value;

	ret = max77818_i2c_reg_read16(fgDev,
				      MAX77818_REG_FG_CONFIG,
				      &value);
	if (ret) {
		printf("%s: Failed to read current FGCC state\n", __func__);
		return ret;
	}
	value &= MAX77818_FG_CONFIG__FGCC_MASK;
	*state = (value == MAX77818_FG_CONFIG__FGCC_ENABLED);

	return 0;
}

int max77818_restore_fgcc(void)
{
	if (fgcc_restore_state) {
		printf("Restoring FGCC state (re-enabling)\n");
		return max77818_enable_fgcc(true);
	}
	else {
		printf("FGCC state not to be enabled (was disabled)\n");
		return 0;
	}
}
int max77818_enable_fgcc(bool enabled)
{
	int ret;
	u16 value;

	value = (enabled ? MAX77818_FG_CONFIG__FGCC_ENABLED :
			   MAX77818_FG_CONFIG__FGCC_DISABLED);

	ret = max77818_i2c_reg_write16(fgDev, MAX77818_REG_FG_CONFIG,
				       MAX77818_FG_CONFIG__FGCC_MASK,
				       value);
	if (ret) {
		printf("%s: Failed to write FGCC mode\n", __func__);
		return ret;
	}

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
	case MAX77818_DETAILS_0__BAT_PRECENT:
		printf("Battery is precent\n");
		break;
	case MAX77818_DETAILS_0__BAT_NOT_PRECENT:
		printf("Battery is NOT precent\n");
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
	case ILIM_1P5_A:
		ilim_config_value = MAX77818_CHG_CNFG_09__CHGIN_ILIM_1P5_A;
		break;
	case ILIM_2P8_A:
		ilim_config_value = MAX77818_CHG_CNFG_09__CHGIN_ILIM_2P8_A;
		break;
	default:
		/* Invalid value, just set default 1.5A */
		ilim_config_value = MAX77818_CHG_CNFG_09__CHGIN_ILIM_1P5_A;
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
				       MAX77818_CHG_CNFG_10__WCIN_ILIM__1P26_A);
}

int max77818_enable_safeout1(void)
{
	int ret;
	u8 val;

	if (!idDev)
		return -ENODEV;

	ret = dm_i2c_read(idDev, MAX77818_REG_SAFEOUTCTRL, &val, 1);
	if (ret)
		return ret;

	val |= MAX77818_SAFEOUTCTRL_ENSAFEOUT1;

	return dm_i2c_write(idDev, MAX77818_REG_SAFEOUTCTRL, &val, 1);
}

static int zs_do_get_battery_charge_status(cmd_tbl_t *cmdtp,
					   int flag,
					   int argc,
					   char * const argv[])
{
	int ret;

	if (!bus || !chDev || !fgDev || !idDev) {
		ret = max77818_init_device();
		if(ret)
			return ret;
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

	if (argc != 2) {
		printf("Usage: set_otg_power <on | off>\n");
		return -1;
	}

	if ((strcmp(argv[1], "on") != 0) && (strcmp(argv[1], "off") != 0)) {
		printf("Usage: set_otg_power <on | off>\n");
		return -1;
	}

	if (!bus || !chDev || !fgDev || !idDev) {
		ret = max77818_init_device();
		if(ret)
			return ret;
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

	if (argc != 1) {
		printf("Usage: set_fastcharge_current_2P8_A\n");
		return -1;
	}

	if (!bus || !chDev || !fgDev || !idDev) {
	ret = max77818_init_device();
	if(ret)
		return ret;
	}

	ret = max77818_set_fast_charge_current(chDev, FASTCHARGE_1P5_A);
	if (ret) {
		printf("Failed to set fast charge current: %d\n", ret);
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

	if (argc != 1) {
		printf("Usage: set_fastcharge_current_2P8_A\n");
		return -1;
	}

	if (!bus || !chDev || !fgDev || !idDev) {
		ret = max77818_init_device();
		if(ret)
			return ret;
	}

	ret = max77818_set_fast_charge_current(chDev, FASTCHARGE_2P8_A);
	if (ret) {
		printf("Failed to set fast charge current: %d\n", ret);
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

	if (argc != 1) {
		printf("Usage: set_charge_termination_voltage\n");
		return -1;
	}

	if (!bus || !chDev || !fgDev || !idDev) {
		ret = max77818_init_device();
		if(ret)
			return ret;
	}

	ret = max77818_set_charge_termination_voltage(chDev);
	if (ret) {
		printf("Failed to set charge termination voltage: %d\n", ret);
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

	if (argc != 1) {
		printf("Usage: set_pogo_input_current_limit\n");
		return -1;
	}

	if (!bus || !chDev || !fgDev || !idDev) {
		ret = max77818_init_device();
		if(ret)
			return ret;
	}

	ret = max77818_set_pogo_input_current_limit(chDev, ILIM_1P5_A);
	if (ret) {
		printf("Failed to set pogo input current limit: %d\n", ret);
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

	if (argc != 1) {
		printf("Usage: set_pogo_input_current_limit\n");
		return -1;
	}

	if (!bus || !chDev || !fgDev || !idDev) {
		ret = max77818_init_device();
		if(ret)
			return ret;
	}

	ret = max77818_set_pogo_input_current_limit(chDev, ILIM_2P8_A);
	if (ret) {
		printf("Failed to set pogo input current limit: %d\n", ret);
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

	if (argc != 1) {
		printf("Usage: set_usbc_input_current_limit\n");
		return -1;
	}

	if (!bus || !chDev || !fgDev || !idDev) {
		ret = max77818_init_device();
		if(ret)
			return ret;
	}

	ret = max77818_set_usbc_input_current_limit(chDev);
	if (ret) {
		printf("Failed to turn OTG power off: %d\n", ret);
		return ret;
	}

	return 0;
}
U_BOOT_CMD(
	max77818_set_usbc_input_current_limit, 1, 1, zs_do_set_usbc_input_current_limit,
	"Set max USB-C input current (1.3A)",
	"Set max charge input current limit to 1260 (1.3A) for the USB-C charge input"
);
