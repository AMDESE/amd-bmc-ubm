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
}

#define COMMAND_BOARD_ID      ("/sbin/fw_printenv -n board_id")
#define COMMAND_BOARD_ID_LEN  (3)
#define COMMAND_POR_RST       ("/sbin/fw_printenv -n por_rst")
#define COMMAND_POR_RST_LEN   (5)

#define BP_CONF_FILE          ("/var/lib/misc/ubm.conf")
#define BP_CONF_END           (0xFF)

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

        if (set_i2c_mux(BP_MUX0_ADDR, mux_port[3]) < SUCCESS) {
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

void bp_config(int reg_cnt)
{
    int i, j, k;

    for (i = 0; i < BP_MUX1_MAX_PORT; i++)
    {
        // Enable BP Bus in Mux 1
        if (set_i2c_mux(BP_MUX1_ADDR, mux_port[i]) < SUCCESS) {
            sd_journal_print(LOG_ERR, "Error: setting Mux1 %x \n",BP_MUX1_ADDR);
            return;
        }
        for (j = 0; j < BP_MUX2_MAX_PORT; j++) {
            // Enable BP Bus in Mux 2
            if (set_i2c_mux(BP_MUX2_ADDR, mux_port[j]) < SUCCESS) {
                sd_journal_print(LOG_ERR, "Error: setting Mux2 %x \n",BP_MUX2_ADDR);
                return;
            }
            if(reg_cnt == 0) {
                if (set_i2c(PSOC_CTL_ADDR, CTL_REG_CFG_DISABLE, BP_CFG_DISABLE) < SUCCESS) {
                    sd_journal_print(LOG_ERR, "Error: setting PSOC Reg 0x%x \n", CTL_REG_CFG_DISABLE);
                    return;
                }
                sd_journal_print(LOG_INFO, "Disabled BP for Port %d %d \n", i, j);
            }
            else {
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
                sd_journal_print(LOG_INFO, "Configured BP on Port %d %d \n", i, j);
            }
        }
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
            bp_file >> std::hex >> reg;
            bp_file >> std::hex >> data;
            if (reg == BP_CONF_END) // end of config file
                break;
            bp_reg_offset[ret]=reg;
            bp_reg_data[ret]=data;
            ret++;;
            sd_journal_print(LOG_INFO, " %s(%d): Reg 0x%x , Data 0x%x \n", BP_CONF_FILE, ret, reg, data);
            ret++;
            if(ret >= CTL_MAX_REG) {
                ret = CTL_MAX_REG;
                break;
            }
        }
    }
    bp_file.close();
    return ret;
}

int main(int argc, char **argv)
{
    FILE *pf;
    char data[COMMAND_POR_RST_LEN];
    unsigned int board_id = 0;
    std::stringstream ss;
    int reg_cnt=0;

    // Check for Power On Reset
    pf = popen(COMMAND_POR_RST,"r");
    if(pf)
    {
        // Get the data from the process execution
        if (fgets(data, COMMAND_POR_RST_LEN, pf))
            sd_journal_print(LOG_INFO, "POR RST: %s\n", data);
    }
    pclose(pf);
    if (strncmp(data, "true", 4) != 0)
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
    pclose(pf);

    switch (board_id)
    {
        case PURICO:
        case PURICO_1:
        case PURICO_2:
        case VOLCANO:
        case VOLCANO_1:
        case VOLCANO_2:
            sd_journal_print(LOG_INFO, "Lenovo Platform: Configure BP  \n");
            if(bp_open_dev() == SUCCESS) {
                reg_cnt = bp_read_conf();
                bp_config(reg_cnt);
            }
            bp_close_dev();
            break;
        default:
            break;
    }

}

