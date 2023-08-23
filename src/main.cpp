#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <phosphor-logging/log.hpp>
#include "ubm_common.h"

extern "C"
{
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
}

#define BP_DEBUG                1

#define COMMAND_BOARD_ID         ("/sbin/fw_printenv -n board_id")
#define COMMAND_BOARD_ID_LEN     (3)
#define COMMAND_POR_RST          ("/sbin/fw_printenv -n por_rst")
#define COMMAND_POR_RST_LEN      (5)
#define COMMAND_POR_RST_RSP_LEN  (4)

#define BP_CONF_FILE          ("/var/lib/misc/ubm.conf")
#define BP_CONF_END           (0xFF)

#define PDB_EEPROM    ("/sys/class/i2c-dev/i2c-250/device/250-0053/eeprom")


//Lenovo Platforms
constexpr auto PURICO    = 106;  //0x6A
constexpr auto PURICO_1  = 114;  //0x72
constexpr auto PURICO_2  = 115;  //0x73
constexpr auto VOLCANO   = 107;  //0x6B
constexpr auto VOLCANO_1 = 116;  //0x74
constexpr auto VOLCANO_2 = 117;  //0x75

static int fd = FAILURE;
static int mux_port[4]={MUX_ENABLE_PORT0, MUX_ENABLE_PORT1, MUX_ENABLE_PORT2, MUX_ENABLE_PORT3};
static int bp_reg_offset[CTL_MAX_REG];
static int bp_reg_data[CTL_MAX_REG];

const BP_Info BP_Table_List[] =
{
        /* BP Name in FRU,                  BP ID (BP Type Code),           BP Total SEP,       BP Total Bay        BP Type,                BP Group ID         HFC       UBM   */
        {"None",                            BP_ID_NONE,                     BP_TOTAL_SEP_0,     BP_TOTAL_BAY_8,     BP_TYPE_ANYBAY,         BP_Group_ID_4,      {0, 0},   0     },
        {"2U 2.5\" Anybay 8-Bay BP",        BP_ID_2U_2_5_Anybay_8_Bay,      BP_TOTAL_SEP_2,     BP_TOTAL_BAY_8,     BP_TYPE_ANYBAY,         BP_Group_ID_4,      {0, 5},   7     },
        {"2U Volcano U.3 8-Bay BP",         BP_ID_2U_U3_Anybay_8_Bay,       BP_TOTAL_SEP_1,     BP_TOTAL_BAY_8,     BP_TYPE_NVME,           BP_Group_ID_4,      {0, 5},   7     },
        {"2U Volcano E3.S 4-Bay BP",        BP_ID_2U_E3S_Anybay_4_Bay,      BP_TOTAL_SEP_1,     BP_TOTAL_BAY_4,     BP_TYPE_NVME,           BP_Group_ID_4,      {0, 5},   4     },
};
const int BP_Table_List_Count = (sizeof(BP_Table_List) / sizeof(BP_Table_List[0]));


/*
 * Initialization step, where Opening the i2c device file.
 */
int set_i2c_mux(int addr, int data)
{
    if (ioctl(fd, I2C_SLAVE, addr) < SUCCESS) {
        sd_journal_print(LOG_ERR, "Error: ioctl for Mux %x \n", addr);
        return FAILURE;
    }
    if (i2c_smbus_write_byte_data(fd, MUX_REG, data) != 0) {
        sd_journal_print(LOG_ERR, "Error: Failed to enable Mux %x \n", addr);
        return FAILURE;
    }
    return SUCCESS;
}

int set_i2c(int addr, int reg, int data)
{
    if (ioctl(fd, I2C_SLAVE, addr) < SUCCESS) {
        sd_journal_print(LOG_ERR, "Error: ioctl for i2c addr %x \n", addr);
        return FAILURE;
    }
    if (i2c_smbus_write_byte_data(fd, reg, data) != 0) {
        sd_journal_print(LOG_ERR, "Error: Failed to write to i2c addr %x \n", addr);
        return FAILURE;
    }
    return SUCCESS;
}

int bp_open_dev(void)
{

    char i2c_devname[FILEPATHSIZE];

    snprintf(i2c_devname, FILEPATHSIZE, "/dev/i2c-%d", BP_I2C_BUS);
    if (fd < SUCCESS) {
        fd = open(i2c_devname, O_RDWR);
        if (fd < SUCCESS) {
            sd_journal_print(LOG_ERR, "Error: Failed to open i2c device %s\n", i2c_devname);
            return FAILURE;
        }

        if (set_i2c_mux(BP_MUX0_ADDR, mux_port[BP_MUX0_PORT]) < SUCCESS) {
            sd_journal_print(LOG_ERR, "Error: setting Mux %s addr %x \n", i2c_devname, BP_MUX0_ADDR);
            return FAILURE;
        }
    }
    else {
        sd_journal_print(LOG_ERR, "Error: device %s is already open \n", i2c_devname);
    }

    return SUCCESS;
}

int bp_close_dev(void)
{
    if (fd >= 0) {
        close(fd);
    }

    fd = FAILURE;
    return SUCCESS;
}

void psoc_set_reg(int reg_cnt)
{
    int k;
    if(reg_cnt == 0) {
        // No conf file, disable PSOC
        if (set_i2c(PSOC_CTL_ADDR, CTL_REG_CFG_DISABLE, BP_CFG_DISABLE) < SUCCESS) {
            sd_journal_print(LOG_ERR, "Error: setting PSOC Reg 0x%x \n", CTL_REG_CFG_DISABLE);
            return;
        }
    }
    else {
        // set BP PSOC registers
        for(k = 0; k < reg_cnt; k++)  {
            if (set_i2c(PSOC_CTL_ADDR, bp_reg_offset[k], bp_reg_data[k]) < SUCCESS) {
                sd_journal_print(LOG_ERR, "Error: setting PSOC Reg 0x%x \n", bp_reg_offset[k]);
                return;
            }
        }

        // done with BP config
        if (set_i2c(PSOC_CTL_ADDR, CTL_REG_CFG_ENABLE, BP_CFG_ENABLE) < SUCCESS) {
            sd_journal_print(LOG_ERR, "Error: setting i2c Addr 0x%x , Reg 0x%x \n", PSOC_CTL_ADDR, CTL_REG_CFG_ENABLE);
            return;
        }
    }
}

void bp_config(int reg_cnt)
{
    int i;

    for (i = 0; i < BP_MUX1_MAX_PORT; i++)
    {
        // Enable BP Bus in Mux 1
        if (set_i2c_mux(BP_MUX1_ADDR, mux_port[i]) < SUCCESS) {
            sd_journal_print(LOG_ERR, "Error: setting Mux1 %x \n",BP_MUX1_ADDR);
            return;
        }
        // Enable BP Bus in Mux 2, Port 0
        sd_journal_print(LOG_INFO, "Configure BP on Port %d %d \n", i, BP_MUX2_PORT_0);
        if (set_i2c_mux(BP_MUX2_ADDR, mux_port[BP_MUX2_PORT_0]) < SUCCESS) {
            sd_journal_print(LOG_ERR, "Error: setting Mux2 %x \n",BP_MUX2_ADDR);
            return;
        }
        psoc_set_reg(reg_cnt);
        // Enable BP Bus in Mux 2, Port 1
        sd_journal_print(LOG_INFO, "Configure BP on Port %d %d \n", i, BP_MUX2_PORT_1);
        if (set_i2c_mux(BP_MUX2_ADDR, mux_port[BP_MUX2_PORT_1]) < SUCCESS) {
            sd_journal_print(LOG_ERR, "Error: setting Mux2 %x \n",BP_MUX2_ADDR);
            return;
        }
        psoc_set_reg(reg_cnt);
    }

    return;
}

int bp_read_conf()
{
    std::ifstream bp_file;
    std::stringstream ss;
    unsigned int reg, data;
    int ret=0;

    bp_file.open(BP_CONF_FILE);
    if ( bp_file.is_open() ) {
        while ( bp_file ) {
            //Read Register offset
            bp_file >> std::hex >> reg;
            if ((reg < CTL_REG_CFG_VAL) ||
                (reg >= CTL_MAX_REG)) {
                if (reg != BP_CONF_END) // check end of config file
                    sd_journal_print(LOG_ERR, "Error: Reading %s in line %d for Register Value \n", BP_CONF_FILE, ret+1);
                break;
            }
            //Read Data
            bp_file >> std::hex >> data;
            if ((data == NULL) ||
                (data > BP_CFG_DISABLE)) {
                sd_journal_print(LOG_ERR, "Error: Reading %s in line %d for Data Value \n", BP_CONF_FILE, ret+1);
                break;
            }
            bp_reg_offset[ret]=reg;
            bp_reg_data[ret]=data;
            ret++;;
            sd_journal_print(LOG_INFO, " %s(%d): Reg 0x%x , Data 0x%x \n", BP_CONF_FILE, ret, reg, data);
            if(ret >= CTL_MAX_REG) {
                ret = CTL_MAX_REG;
                break;
            }
        }
    }
    bp_file.close();
    return ret;
}

/* Check the BP auto-configuration register offset to see whether to update the value or not.
 * If any of the register is changed, then BP_CONTROL_REGISTER_AUTO_CONFIG_ENABLE need to be set to 0xBE regardless of the current value.
 * arg: bus_name (i2c bus name for BP)
 * arg: which_bp (BP connector offset)
 * arg: which_sep (which SEP in a BP)
 * arg: offset (BP SEP register offset)
 * arg: value (data to be set)
 */
int Check_BP_Auto_Configuration_Register(char* bus_name, uint8_t which_bp, uint8_t which_sep, uint8_t offset, uint8_t value)
{
    static bool Is_Auto_Config_Value_Updated[BP_TOTAL_CONNECTOR][BP_TOTAL_SEP_3] = {{0}};
    size_t write_count   = 1;
    size_t read_count    = 1;
    uint8_t  write_data[2] = {0};
    uint8_t  read_data[2]  = {0};
    int    ret           = -1;
    int fd = -1;

    if(BP_DEBUG) sd_journal_print(LOG_INFO,"%s bus:%s bp:%d  sep:%d  offset:0x%.2x  value:0x%.2x\n", __FUNCTION__, bus_name, which_bp, which_sep, offset,value);
    fd = open(bus_name, O_RDWR);
    if (fd < SUCCESS) {
        sd_journal_print(LOG_ERR, "Error: Failed to open i2c device %s\n", bus_name);
        return FAILURE;
    }

    if (ioctl(fd, I2C_SLAVE, BP_SLAVE_ADDR_SEP_CONTROL_REG) < SUCCESS) {
        sd_journal_print(LOG_ERR, "Error: %s ioctl for i2c addr %x \n", bus_name, BP_SLAVE_ADDR_SEP_CONTROL_REG);
        close(fd);
        return FAILURE;
    }

    if (i2c_smbus_write_byte_data(fd, offset, value) != 0) {
        sd_journal_print(LOG_ERR, "Error:%s Failed to write to i2c addr %x offset:%x\n",bus_name, BP_SLAVE_ADDR_SEP_CONTROL_REG, offset);
        close(fd);
        return FAILURE;
    }

    close(fd);

    return SUCCESS;
}




/* Auto-Configuration Step 1 Range of values are 1-8; by 4 or by 8 group.
 * arg: bus_name (i2c bus name for BP)
 * arg: which_bp (BP connector offset)
 * arg: which_sep (which SEP in a BP)
 */
int Check_BP_Group_ID(char* bus_name, uint8_t which_bp, uint8_t which_sep)
{
    uint8_t offset = BP_CONTROL_REGISTER_GROUP_ID;
    uint8_t value  = 0x01;
    int   ret    = FAILURE;

    if (BP_Group_ID_4 == BP_Present_List[which_bp].BP_Group_ID)
    {
        value = (which_sep + 1);
    }
    else
    {
        value = ((which_sep * 2) + 1);
    }

    ret = Check_BP_Auto_Configuration_Register(bus_name, which_bp, which_sep, offset, value);
    return ret;
}

/* Auto-Configuration Step 2 This is the PCIe slot information.
 * This step is skipped in SAS/SATA only BP.
 * arg: bus_name (i2c bus name for BP)
 * arg: which_bp (BP connector offset)
 * arg: which_sep (which SEP in a BP)
 */
int Check_BP_Slot_ID(char* bus_name, uint8_t which_bp, uint8_t which_sep)
{
    uint8_t offset = BP_CONTROL_REGISTER_SLOT_ID;
    uint8_t value  = (0x40 + (BP_Present_List[which_bp].BP_Group_ID * which_sep));
    int ret      = FAILURE;

    ret = Check_BP_Auto_Configuration_Register(bus_name, which_bp, which_sep, offset, value);

    return ret;
}

/* Auto-Configuration Step 3 This is the physical backplane Bay location in the enclosure.
 * This step is skipped in SAS/SATA only BP.
 * arg: bus_name (i2c bus name for BP)
 * arg: which_bp (BP connector offset)
 * arg: which_sep (which SEP in a BP)
 */
int Check_BP_Bay_ID(char* bus_name, uint8_t which_bp, uint8_t which_sep)
{
    uint8_t offset = BP_CONTROL_REGISTER_BAY_ID;
    uint8_t value  = (BP_Present_List[which_bp].BP_Group_ID * which_sep);
    int ret      = FAILURE;

    ret = Check_BP_Auto_Configuration_Register(bus_name, which_bp, which_sep, offset, value);

    return ret;
}

/* Auto-Configuration Step 4 This register describe the backplane configuration.
 * arg: bus_name (i2c bus name for BP)
 * arg: which_bp (BP connector offset)
 * arg: which_sep (which SEP in a BP)
 */
int Check_BP_Backplane_Information(char* bus_name, uint8_t which_bp, uint8_t which_sep)
{
    uint8_t offset = BP_CONTROL_REGISTER_BACKPLANE_INFO;
    uint8_t value  = (which_bp + 1);
    int ret      = FAILURE;

    ret = Check_BP_Auto_Configuration_Register(bus_name, which_bp, which_sep, offset, value);

    return ret;
}

/* Auto-Configuration Step 5 Indicates the number of slots/bays on the backplane.
 * Each SEP on a backplane receive the same number of slots.
 * arg: bus_name (i2c bus name for BP)
 * arg: which_bp (BP connector offset)
 * arg: which_sep (which SEP in a BP)
 */
int Check_BP_Number_of_Slots(char* bus_name, uint8_t which_bp, uint8_t which_sep)
{
    uint8_t offset = BP_CONTROL_REGISTER_NUM_OF_SLOTS;
    uint8_t value  = BP_Present_List[which_bp].BP_Total_Bay;
    int ret      = FAILURE;

    ret = Check_BP_Auto_Configuration_Register(bus_name, which_bp, which_sep, offset, value);

    return ret;
}

/* Auto-Configuration Step 6 Indicating the starting physical backplane bay location for each SEP.
 * arg: bus_name (i2c bus name for BP)
 * arg: which_bp (BP connector offset)
 * arg: which_sep (which SEP in a BP)
 */
int Check_BP_Starting_Slot_Number(char* bus_name, uint8_t which_bp, uint8_t which_sep)
{
    uint8_t offset = BP_CONTROL_REGISTER_START_SLOT_NUM;
    uint8_t value  = (BP_Present_List[which_bp].BP_Group_ID * which_sep);
    int ret      = FAILURE;

    ret = Check_BP_Auto_Configuration_Register(bus_name, which_bp, which_sep, offset, value);

    return ret;
}

/* Auto-Configuration Step 7 Indicate type of system and supported management protocol.
 * arg: bus_name (i2c bus name for BP)
 * arg: which_bp (BP connector offset)
 * arg: which_sep (which SEP in a BP)
 */
int Check_BP_Starting_Host_Facing_Connector_Identity(char* bus_name, uint8_t which_bp, uint8_t which_sep)
{
    uint8_t offset = BP_CONTROL_REGISTER_START_HFC_IDENTITY;
    uint8_t value  = (BP_Present_List[which_bp].BP_HFC[which_sep]);
    int ret      = FAILURE;

    ret = Check_BP_Auto_Configuration_Register(bus_name, which_bp, which_sep, offset, value);

    return ret;
}

/* Auto-Configuration Step 8 Indicate type of system and supported management protocol.
 * arg: bus_name (i2c bus name for BP)
 * arg: which_bp (BP connector offset)
 * arg: which_sep (which SEP in a BP)
 */
int Check_BP_System_Type_Managment_Protocol_Support(char* bus_name, uint8_t which_bp, uint8_t which_sep)
{
    uint8_t offset = BP_CONTROL_REGISTER_SYSTEM_MGMT_PROTOCOL;
    uint8_t value  = (BP_Present_List[which_bp].BP_UBM);
    int ret      = FAILURE;

    ret = Check_BP_Auto_Configuration_Register(bus_name, which_bp, which_sep, offset, value);

    return ret;
}

/* Auto-Configuration Step 9 Auto-configuration enable register is set by the BMC to 0xBE (enable).
 * STEPS 1-8 MUST BE PERFORMED PRIOR TO EXECUTING STEP 9.
 * arg: bus_name (i2c bus name for BP)
 * arg: which_bp (BP connector offset)
 * arg: which_sep (which SEP in a BP)
 */
int Check_BP_Enable_Auto_Configuration_Register(char* bus_name, uint8_t which_bp, uint8_t which_sep)
{
    uint8_t offset = BP_CONTROL_REGISTER_AUTO_CONFIG_ENABLE;
    uint8_t value  = 0xBE;
    int ret      = FAILURE;

    ret = Check_BP_Auto_Configuration_Register(bus_name, which_bp, which_sep, offset, value);

    return ret;
}

/* Perform BP auto-configuration task with a total of 9 steps as suggested in BP SEP FW specification Chapter 5.
 * This is to ensure that the Disk status's valid bit (BIT 7) is high.
 * arg: bus_name (i2c bus name for BP)
 * arg: which_bp (BP connector offset)
 * arg: which_sep (which SEP in a BP)
 */
int BP_Auto_Configuration_Handler(char* bus_name, uint8_t which_bp, uint8_t which_sep)
{
    int ret = FAILURE;

    ret = Check_BP_Group_ID(bus_name, which_bp, which_sep);
    if (0 != ret)
    {
        return ret;
    }

    if (BP_TYPE_SAS_SATA != BP_Present_List[which_bp].BP_Type)
    {
        ret = Check_BP_Slot_ID(bus_name, which_bp, which_sep);
        if (0 != ret)
        {
            return ret;
        }

        ret = Check_BP_Bay_ID(bus_name, which_bp, which_sep);
        if (0 != ret)
        {
            return ret;
        }
    }

    ret = Check_BP_Backplane_Information(bus_name, which_bp, which_sep);
    if (0 != ret)
    {
        return ret;
    }

    ret = Check_BP_Number_of_Slots(bus_name, which_bp, which_sep);
    if (0 != ret)
    {
        return ret;
    }

    ret = Check_BP_Starting_Slot_Number(bus_name, which_bp, which_sep);
    if (0 != ret)
    {
        return ret;
    }

    ret = Check_BP_Starting_Host_Facing_Connector_Identity(bus_name, which_bp, which_sep);
    if (0 != ret)
    {
        return ret;
    }

    ret = Check_BP_System_Type_Managment_Protocol_Support(bus_name, which_bp, which_sep);
    if (0 != ret)
    {
        return ret;
    }

    ret = Check_BP_Enable_Auto_Configuration_Register(bus_name, which_bp, which_sep);
    if (0 != ret)
    {
        return ret;
    }
    return 0;
}

/* All BP tasks that require access to BP SEP (pSoC) before accessing BP register should be added in this function.
 * arg: which_bp (BP connector offset)
 */
int BP_SEP_Init_Handler(uint8_t which_bp)
{
    char bus_name[16] = "";
    int  i           = 0;
    int  ret         = FAILURE;

    if (BP_Present_List[which_bp].BP_Total_SEP == 0)
        return ret;

    if (BP_TOTAL_SEP_1 < BP_Present_List[which_bp].BP_Total_SEP)
    {
        for (i = 0; i < BP_Present_List[which_bp].BP_Total_SEP; i++)
        {
            snprintf(bus_name, sizeof(bus_name),PREFIX_BPBUS, 55 + (which_bp * 2) + i);
            if(BP_DEBUG) sd_journal_print(LOG_INFO,"%s:%d  bus:%s\n", __FUNCTION__, __LINE__ , bus_name);
            ret = BP_Auto_Configuration_Handler(bus_name, which_bp, i);
            if (SUCCESS != ret)
            {
                sd_journal_print(LOG_ERR,"[%s][%d] Failed Auto-Config on BP [%d] with return code [0x%x]!\n", __FUNCTION__, __LINE__, which_bp, ret);
                return ret;
            }
        }
    }
    else
    {
        snprintf(bus_name, sizeof(bus_name),PREFIX_BPBUS, 55 + (which_bp * 2));
        ret = BP_Auto_Configuration_Handler(bus_name, which_bp, i);
        if (SUCCESS != ret)
        {
            sd_journal_print(LOG_ERR,"[%s][%d] Failed Auto-Config on BP [%d] with return code [0x%x]!\n", __FUNCTION__, __LINE__, which_bp, ret);
            return ret;
        }
    }

    return SUCCESS;
}

/* E3.s BP tasks that require access to BP SEP (pSoC) before accessing BP register should be added in this function.
 * arg: which_bp (BP connector offset)
 */

int B3S_SEP_Init_Handler(uint8_t which_bp)
{
    char bus_name[16] = "";
    int  i           = 0;
    int  ret         = FAILURE;
    int  E3SBusNum[]={55,56,61,62,63,64};


    if (BP_Present_List[which_bp].BP_Total_SEP == 0)
        return ret;

    snprintf(bus_name, sizeof(bus_name),PREFIX_BPBUS, E3SBusNum[which_bp] );

    ret = BP_Auto_Configuration_Handler(bus_name, which_bp, i);
    if (SUCCESS != ret)
    {
        sd_journal_print(LOG_ERR,"[%s][%d] Failed Auto-Config on BP [%d] with return code [0x%x]!\n", __FUNCTION__, __LINE__, which_bp, ret);
        return ret;
    }

    return SUCCESS;
}



/* Check BP FRU info against BP_Table_List's BP_Name field.
 * arg: which_bp (BP connector offset)
 * arg: which_fru path (corresponding BP FRU path)
 */
int Check_BP_FRU_Info(uint8_t which_bp, const char *fru_path)
{

    FILE *fp = NULL;
    int       nRet;
    char  bp_fru_info[BP_FRU_BOARD_PRODUCT_SIZE] = "";

    if ((fp = fopen (fru_path, "r")) == NULL)
    {
        sd_journal_print(LOG_ERR,"fopen %s fail!!\n",fru_path);
        return FAILURE;
    }

    nRet = fseek( fp, BP_FRU_BOARD_PRODUCT_OFFSET, SEEK_SET );
    if (nRet != 0)
    {
        fclose( fp );
        sd_journal_print(LOG_ERR,"fseek %s fail!!\n",fru_path);
        return FAILURE;
    }
    fread( bp_fru_info, 1, BP_FRU_BOARD_PRODUCT_SIZE, fp );
    fclose(fp);
    if(BP_DEBUG) sd_journal_print(LOG_INFO,"%s:%d  %s\n",__FUNCTION__, __LINE__, bp_fru_info);

    for (int i = 0; i < BP_Table_List_Count; i++)
    {
        if (NULL != strstr(bp_fru_info, BP_Table_List[i].BP_Name))
        {
            BP_Present_List[which_bp] = BP_Table_List[i];
            sd_journal_print(LOG_INFO,"BP#%d [%s] detected with [%d] SEP.\n", which_bp, BP_Present_List[which_bp].BP_Name, BP_Present_List[which_bp].BP_Total_SEP);
            return SUCCESS;
        }
    }

    return FAILURE;
}

/* Check PDB FRU info against BP_Table_List's BP_Name field.
 * arg: which_bp (BP connector offset)
 * arg: which_fru path (corresponding BP FRU path)
 */
int Check_PDB_FRU_Info(const char *fru_path)
{

    FILE *fp = NULL;
    int       nRet;
    char  bp_fru_info[BP_FRU_BOARD_PRODUCT_SIZE] = "";

    if ((fp = fopen (fru_path, "r")) == NULL)
    {
        sd_journal_print(LOG_ERR,"fopen %s fail!!\n",fru_path);
        return FAILURE;
    }

    nRet = fseek( fp, BP_FRU_BOARD_PRODUCT_OFFSET, SEEK_SET );
    if (nRet != 0)
    {
        fclose( fp );
        sd_journal_print(LOG_ERR,"fseek %s fail!!\n",fru_path);
        return FAILURE;
    }
    fread( bp_fru_info, 1, BP_FRU_BOARD_PRODUCT_SIZE, fp );
    fclose(fp);

    if(BP_DEBUG) sd_journal_print(LOG_INFO,"%s:%d  %s\n",__FUNCTION__, __LINE__, bp_fru_info);

    if (NULL != strstr(bp_fru_info, "Volcano E3.S PDB"))
    {
        sd_journal_print(LOG_INFO,"Found E3.S PDB .\n");
        return SUCCESS;
    }

    return FAILURE;
}



/* All BP tasks need to be done before monitoring BP status should be added in this function.
 * Tasks that require access to BP SEP should be added in BP_SEP_Init_Handler.
 * arg: which_bp (BP connector offset)
 * arg: which_fru path (corresponding BP FRU path)
  */
int BP_Init_Handler(uint8_t which_bp, const char *fru_path)
{
    int ret = FAILURE;

    if(which_bp >= BP_TOTAL_CONNECTOR)
        return ret;

    if (0 == BP_Present_List[which_bp].BP_Total_SEP)
    {
        ret = Check_BP_FRU_Info(which_bp, fru_path);
        if (SUCCESS != ret)
        {
            return ret;
        }
    }

    ret = BP_SEP_Init_Handler(which_bp);
    return ret;
}

/* E3.s BP tasks need to be done before monitoring BP status should be added in this function.
 * Tasks that require access to BP SEP should be added in BP_SEP_Init_Handler.
 * arg: which_bp (BP connector offset)
 * arg: which_fru path (corresponding BP FRU path)
  */
int E3S_Init_Handler(uint8_t which_bp, const char *fru_path)
{
    int ret = FAILURE;

    if(which_bp >= BP_TOTAL_CONNECTOR)
        return ret;

    if (0 == BP_Present_List[which_bp].BP_Total_SEP)
    {
        ret = Check_BP_FRU_Info(which_bp, fru_path);
        if (SUCCESS != ret)
        {
            return ret;
        }
    }

    ret = B3S_SEP_Init_Handler(which_bp);
    return ret;
}


/* BP auto configuration entry point.
 * Tasks that require access to BP SEP should be added in BP_SEP_Init_Handler.
 * arg: BP config count
 * arg: BP config parameter
 */
void BP_auto_config(uint8_t BP_Config_List_Count, BP_Config *list)
{
    for (int8_t i = 0; i < BP_Config_List_Count; i++)
    {
        if( access( list[i].BP_EEPROM , F_OK ) != -1 ) {
            sd_journal_print(LOG_INFO,"%s check OK!!\n",list[i].BP_EEPROM);
            BP_Init_Handler(list[i].BP_Connector_Offset, list[i].BP_EEPROM);
        } else {
            sd_journal_print(LOG_INFO,"%s check Fail!!\n",list[i].BP_EEPROM);
        }
    }
}

/* E3.s auto configuration entry point.
 * Tasks that require access to BP SEP should be added in BP_SEP_Init_Handler.
 * arg: BP config count
 * arg: BP config parameter
 */
void E3S_auto_config(uint8_t BP_Config_List_Count, BP_Config *list)
{
    for (int8_t i = 0; i < BP_Config_List_Count; i++)
    {
        if( access( list[i].BP_EEPROM , F_OK ) != -1 ) {
            sd_journal_print(LOG_INFO,"%s check OK!!\n",list[i].BP_EEPROM);
            E3S_Init_Handler(list[i].BP_Connector_Offset, list[i].BP_EEPROM);
        } else {
            sd_journal_print(LOG_INFO,"%s check Fail!!\n",list[i].BP_EEPROM);
        }
    }
}



int main(int argc, char **argv)
{
    FILE *pf = NULL;
    char data[COMMAND_POR_RST_LEN];
    unsigned int board_id = 0;
    std::stringstream ss;
    int reg_cnt=0;
    BP_Config BP_Config_List[BP_TOTAL_CONNECTOR];

    // Check for Power On Reset
    pf = popen(COMMAND_POR_RST,"r");
    if(pf)
    {
        // Get the data from the process execution
        if (fgets(data, COMMAND_POR_RST_LEN, pf))
            sd_journal_print(LOG_INFO, "POR RST: %s\n", data);
    }
    if(pf)
        pclose(pf);

    if (strncmp(data, "true", COMMAND_POR_RST_RSP_LEN) != 0)
        return 0; //Not a Power On Reset

    // Look for Lenovo systems
    pf = popen(COMMAND_BOARD_ID,"r");
    if(pf)
    {
        // Get the data from the process execution
        if (fgets(data, COMMAND_BOARD_ID_LEN, pf))
        {
            ss << std::hex << (std::string)data;
            ss >> board_id;
            sd_journal_print(LOG_INFO, "Board ID: 0x%x, Board ID String: %s\n", board_id, data);
        }

    }
    if(pf)
        pclose(pf);

    switch (board_id)
    {
        case PURICO:
        case PURICO_1:
        case PURICO_2:
        case VOLCANO:
        case VOLCANO_1:
        case VOLCANO_2:
        {
            sd_journal_print(LOG_INFO, "Lenovo Platform: Configure BP  \n");
            memset(BP_Present_List,      0, sizeof(BP_Present_List));

           if( access( PDB_EEPROM , F_OK ) == 0 )
           {
                sd_journal_print(LOG_INFO,"PDB %s check OK!!\n",PDB_EEPROM);
                if(Check_PDB_FRU_Info(PDB_EEPROM) == SUCCESS)                {

                    uint8_t BP_Config_List_Count = 6;
                    BP_Config_List[0].BP_Connector_Offset = 0;
                    BP_Config_List[0].BP_EEPROM = E3S1_FRU_PATH;
                    BP_Config_List[0].Disk_Start_Index = 0xFF;
                    BP_Config_List[1].BP_Connector_Offset = 1;
                    BP_Config_List[1].BP_EEPROM = E3S2_FRU_PATH;
                    BP_Config_List[1].Disk_Start_Index = 0xFF;
                    BP_Config_List[2].BP_Connector_Offset = 2;
                    BP_Config_List[2].BP_EEPROM = E3S3_FRU_PATH;
                    BP_Config_List[2].Disk_Start_Index = 0xFF;
                    BP_Config_List[3].BP_Connector_Offset = 3;
                    BP_Config_List[3].BP_EEPROM = E3S4_FRU_PATH;
                    BP_Config_List[3].Disk_Start_Index = 0xFF;
                    BP_Config_List[4].BP_Connector_Offset = 4;
                    BP_Config_List[4].BP_EEPROM = E3S5_FRU_PATH;
                    BP_Config_List[4].Disk_Start_Index = 0xFF;
                    BP_Config_List[5].BP_Connector_Offset = 5;
                    BP_Config_List[5].BP_EEPROM = E3S6_FRU_PATH;
                    BP_Config_List[5].Disk_Start_Index = 0xFF;
                    E3S_auto_config(BP_Config_List_Count, BP_Config_List);
                }
           }
            else
           {
                uint8_t BP_Config_List_Count = 3;
                BP_Config_List[0].BP_Connector_Offset = 0;
                BP_Config_List[0].BP_EEPROM = BP1_FRU_PATH;
                BP_Config_List[0].Disk_Start_Index = 0xFF;
                BP_Config_List[1].BP_Connector_Offset = 1;
                BP_Config_List[1].BP_EEPROM = BP2_FRU_PATH;
                BP_Config_List[1].Disk_Start_Index = 0xFF;
                BP_Config_List[2].BP_Connector_Offset = 2;
                BP_Config_List[2].BP_EEPROM = BP3_FRU_PATH;
                BP_Config_List[2].Disk_Start_Index = 0xFF;

                BP_auto_config(BP_Config_List_Count, BP_Config_List);
           }
#if 0
            if(bp_open_dev() == SUCCESS) {
                reg_cnt = bp_read_conf();
                bp_config(reg_cnt);
            }
            bp_close_dev();
#endif
        }
            break;
        default:
            break;
    }
}

