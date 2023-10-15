/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include "cam_nubia_dev.h"
#include "../cam_eeprom/cam_eeprom_soc.h"
#include "../cam_eeprom/cam_eeprom_core.h"



#define CAL_DATA_ADDR    0x70F   //nubia kangxiong  add for CalibrationData in Hi846

static struct kobject *nubia_camera_kobj;
static int torch_switch;
static int bokeh_write;

static struct camera_io_master eeprom_master_info;  //nubia songliang add for CalibrationData
bool is_eeprom_poweron;  //nubia songliang add for CalibrationData



/*ZTEMT: kangxiong add for file_dump--------Start*/
static const char FileDumpPath[]        = "/data/vendor/camera/";

enum file_dump_type {
	CAMERA_FILE_EEPROM_BACK_MAIN     = 0x01,
	CAMERA_FILE_EEPROM_BACK_AUX      = 0x02,
	CAMERA_FILE_EEPROM_FRONT_MAIN	 = 0x03,
	CAMERA_FILE_EEPROM_FRONT_AUX 	 = 0x04,
	CAMERA_FILE_EEPROM_ARCSOFT 	     = 0x05,
	CAMERA_FILE_EEPROM_ARCSOFT_NEW 	 = 0x06,
	CAMERA_FILE_WATERMARK		     = 0x07,
	CAMERA_FILE_AI              	 = 0x08,
	CAMERA_FILE_DEPTH              	 = 0x09,
	CAMERA_FILE_MAX,
};
/*ZTEMT: kangxiong add for file_dump--------End*/

/*ZTEMT: kangxiong add for write calibration--------Start*/
int32_t cam_nubia_eeprom_io_init(struct camera_io_master io_master)
{
	int rc = 0;

	CAM_ERR(CAM_SENSOR, "copy eeprom_master_info");

	eeprom_master_info = io_master;
	return rc;
}
EXPORT_SYMBOL(cam_nubia_eeprom_io_init);
int32_t cam_nubia_eeprom_unlock_write(void)
{
	int rc = 0;
	uint32_t readdata = 0,LensID = 0;
	uint16_t sid_tmp = 0;

	struct cam_sensor_i2c_reg_setting write_setting;
	struct cam_sensor_i2c_reg_array unlock_reg[] =
		{
			{0x8000, 0x00, 0x00, 0x00},
		};

	if(!(eeprom_master_info.cci_client)){
		CAM_ERR(CAM_EEPROM, "eeprom_master_info.cci_client is NULL");
		return -1;
	}
	
	sid_tmp = eeprom_master_info.cci_client->sid;
	eeprom_master_info.cci_client->sid = 0xAC >> 1;//0xBC

//check LensID, er gong has a differnet lock reg
    rc = camera_io_dev_read(&eeprom_master_info, 0x0002,
				&LensID, CAMERA_SENSOR_I2C_TYPE_WORD,
				CAMERA_SENSOR_I2C_TYPE_BYTE,false);
	if (rc < 0)
	{
		CAM_ERR(CAM_EEPROM, "camera_io_dev_read error");
	}
	
//modify regdata if LensID is 0x02
    if(LensID == 2){
        rc = camera_io_dev_read(&eeprom_master_info, unlock_reg->reg_addr,
				&readdata, CAMERA_SENSOR_I2C_TYPE_WORD,
				CAMERA_SENSOR_I2C_TYPE_BYTE,false);
		unlock_reg->reg_data = readdata&0xE;
	}
	CAM_INFO(CAM_EEPROM, " unlock_reg 0x%x 0x%x ", unlock_reg->reg_addr,unlock_reg->reg_data);
	write_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	write_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.delay = 0;
	write_setting.reg_setting = unlock_reg;
	write_setting.size = 1;

	rc = camera_io_dev_write(&eeprom_master_info, &write_setting);
	if (rc < 0)
	{
		CAM_ERR(CAM_EEPROM, "camera_io_dev_write  [unlock] error");
		return -1;
	}
	usleep_range(10000, 11000);
	//for debug CSP
	rc = camera_io_dev_read(&eeprom_master_info, unlock_reg->reg_addr,
				&readdata, CAMERA_SENSOR_I2C_TYPE_WORD,
				CAMERA_SENSOR_I2C_TYPE_BYTE,false);
	if (rc < 0)
	{
		CAM_ERR(CAM_EEPROM, "camera_io_dev_read error");
	}

	eeprom_master_info.cci_client->sid = sid_tmp;
	CAM_ERR(CAM_EEPROM, " read CSP =%x ", readdata);

    return rc;
}

int32_t cam_nubia_eeprom_lock_write(void)
{
	int rc = 0;
	uint32_t readdata = 0,LensID = 0;
	uint16_t sid_tmp = 0;

	struct cam_sensor_i2c_reg_setting write_setting;
	struct cam_sensor_i2c_reg_array lock_reg[] =
		{
			{0x8000, 0x0E, 0x00, 0x00},
		};
	sid_tmp = eeprom_master_info.cci_client->sid;
	eeprom_master_info.cci_client->sid = 0xAC >> 1;//0xBC
	
//check LensID, er gong has a differnet lock reg
	rc = camera_io_dev_read(&eeprom_master_info, 0x0002,
				&LensID, CAMERA_SENSOR_I2C_TYPE_WORD,
				CAMERA_SENSOR_I2C_TYPE_BYTE,false);
	if (rc < 0)
	{
		CAM_ERR(CAM_EEPROM, "camera_io_dev_read error");
	}
//modify regdata if LensID is 0x02
    if(LensID == 2){
        rc = camera_io_dev_read(&eeprom_master_info, lock_reg->reg_addr,
				&readdata, CAMERA_SENSOR_I2C_TYPE_WORD,
				CAMERA_SENSOR_I2C_TYPE_BYTE,false);
		lock_reg->reg_data = readdata|0x01;
	}

	write_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	write_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.delay = 0;
	write_setting.reg_setting = lock_reg;
	write_setting.size = 1;

	CAM_INFO(CAM_EEPROM, " unlock_reg 0x%x 0x%x ", lock_reg->reg_addr,lock_reg->reg_data);
	rc = camera_io_dev_write(&eeprom_master_info, &write_setting);
	if (rc < 0)
	{
		CAM_ERR(CAM_SENSOR, "camera_io_dev_write  [lock] error");
		return -1;
	}
	usleep_range(10000, 11000);

	//for debug CSP
	rc = camera_io_dev_read(&eeprom_master_info, lock_reg->reg_addr,
				&readdata, CAMERA_SENSOR_I2C_TYPE_WORD,
				CAMERA_SENSOR_I2C_TYPE_BYTE,false);
	if (rc < 0)
	{
		CAM_ERR(CAM_EEPROM, "camera_io_dev_read error");
	}
	eeprom_master_info.cci_client->sid = sid_tmp;
	CAM_ERR(CAM_EEPROM, " read CSP =0x%x ", readdata);
	return rc;

}

int32_t cam_nubia_i2c_write_seq(uint32_t addr, uint8_t *data, uint32_t num_byte)
{
	int32_t rc = -EFAULT;
	int write_num = 0;
	int write_width = 32;
	int i = 0;
	int j = 0;

	struct cam_sensor_i2c_reg_array write_buf[32];
	struct cam_sensor_i2c_reg_setting write_setting;

	CAM_ERR(CAM_EEPROM, "%s  ---E", __func__);
	CAM_ERR(CAM_EEPROM, "num_byte = %d", num_byte);

	if (num_byte < 32)
	{
		CAM_ERR(CAM_EEPROM, "%s  num_byte is smaller than 32 ,error", __func__);
		goto END;
	}

	//frist_write_width
	write_width = 32 - (addr % 32);

	for (i = 0; i < num_byte;)
	{
		//update write_width
		if (num_byte - write_num < 32)
		{
			write_width = num_byte - write_num;
		}
		CAM_ERR(CAM_EEPROM, "songliang remain num = %d addr= %x write_width = %d data[%d] = %d",
				num_byte - write_num, addr + i, write_width,write_num,data[write_num]);

		//update write_buf
		for (j = 0; j < write_width; j++)
		{
			write_buf[j].reg_data = data[write_num + j];
			write_buf[j].delay = 0;
			write_buf[j].data_mask = 0;
		}

		write_buf[0].reg_addr = addr + i;
		write_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
		write_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
		write_setting.delay = 0;
		write_setting.reg_setting = write_buf;
		write_setting.size = write_width;

		rc = cam_cci_i2c_write_continuous_table(&eeprom_master_info, &write_setting, CAM_SENSOR_I2C_WRITE_SEQ);
		if ( rc < 0)
		{
			CAM_ERR(CAM_EEPROM, "%s nubia_i2c_write_seq error\n", __func__);
			goto END;
		}

		write_num += write_width;
		i += write_width;
		write_width = 32;

		usleep_range(5000, 6000);
	}

	CAM_ERR(CAM_EEPROM, "%s  total write num = %d\n", __func__, write_num);

END:
	CAM_ERR(CAM_EEPROM, "%s  ---X\n", __func__);
	return rc;
}
/*ZTEMT: kangxiong add for write calibration--------end*/

static struct cam_flash_ctrl *nubia_flash_ctrl;
void nubia_flash_node_save_ctrl(struct cam_flash_ctrl *flash_ctrl)
{
	pr_err("[CAM_NODE] nubia_flash_node_save_ctrl \n");
	nubia_flash_ctrl = flash_ctrl;
}


/*ZTEMT: kangxiong add for torch--------Start*/
static ssize_t torch_switch_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", torch_switch);
}

static ssize_t torch_switch_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	int ret;

	ret = kstrtoint(buf, 10, &torch_switch);
	if (ret < 0)
		return ret;

	if(NULL == nubia_flash_ctrl){
		pr_err("[CAM_NODE] Invalid nubia_flash_ctrl \n");
		return ret;
	}

	cam_flash_switch(nubia_flash_ctrl,torch_switch);


	return count;
}

static struct kobj_attribute torch_switch_attribute =
	__ATTR(torch_switch, 0664, torch_switch_show, torch_switch_store);
/*ZTEMT: kangxiong add for torch--------End*/

/*ZTEMT: kangxiong add for write calibration--------Start*/
static ssize_t eeprom_calibration_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
    if (bokeh_write){
        return sprintf(buf,"%s\0",  "OK");
    }else {
        return sprintf(buf,"%s\0",  "FAIL");
    }
}

static ssize_t eeprom_calibration_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	int ret= 0;
	int times = 0;
	
	if(!is_eeprom_poweron){
		for(times = 0; times <= 3;times++){
            usleep_range(200000, 210000);
			if(is_eeprom_poweron){
               break;
			}
		}
	}
	pr_err("eeprom_calibration_store IN.is_eeprom_poweron = %d,times = %d",is_eeprom_poweron,times);
	
	if (!is_eeprom_poweron){
        return -EINVAL;
	}


	if(!(eeprom_master_info.cci_client)){
		CAM_ERR(CAM_EEPROM, "eeprom_master_info.cci_client is NULL");
		return -EINVAL;
	}

	if (eeprom_master_info.master_type == CCI_MASTER)
	{
		ret = camera_io_init(&eeprom_master_info);
		if (ret)
		{
			pr_err( "cci_init failed");
			return -EINVAL;
		}
	}

	cam_nubia_eeprom_unlock_write();

	usleep_range(10000, 11000);

	ret = cam_nubia_i2c_write_seq(CAL_DATA_ADDR,(uint8_t*)buf, count);

	usleep_range(10000, 11000);

	cam_nubia_eeprom_lock_write();
	camera_io_release(&eeprom_master_info);
    if (ret < 0)
        bokeh_write = 0;
    else
        bokeh_write = 1;

    ret = count;
	return ret;
}


static struct kobj_attribute eeprom_calibration_attribute =
	__ATTR(eeprom_calibration, 0664, eeprom_calibration_show, eeprom_calibration_store);
/*ZTEMT: kangxiong add for write calibration--------end*/


static struct attribute *attrs[] = {
	&torch_switch_attribute.attr,
	&eeprom_calibration_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};


int32_t  cam_nubia_node_init_module(void)
{
	int retval;

	printk("kangxiong add cam_nubia_node_init_module");

	nubia_camera_kobj = kobject_create_and_add("camera", kernel_kobj);
	if (!nubia_camera_kobj)
		return -ENOMEM;

	retval = sysfs_create_group(nubia_camera_kobj, &attr_group);
	if (retval)
		kobject_put(nubia_camera_kobj);

	return retval;

}

void cam_nubia_node_exit_module(void)
{
	pr_err("[CAM_NODE] cam_nubia_node_exit_module \n");
	kobject_put(nubia_camera_kobj);
}


MODULE_DESCRIPTION("CAM NUBIA");
MODULE_LICENSE("GPL v2");

