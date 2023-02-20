#ifndef UBM_COMMON_H
#define UBM_COMMON_H

// BP Conf file
#define BP_CONF_FILE      ("/var/lib/misc/ubm.conf")
#define BP_CONF_END       (0xFF)

// BP I2C Addresses
#define BP_I2C_BUS        (10)
#define BP_MUX0_ADDR      (0x75)
#define BP_MUX1_ADDR      (0x70)
#define BP_MUX2_ADDR      (0x71)
#define PSOC_CTL_ADDR     (0x60)
#define PSOC_STATUS_ADDR  (0x20)
#define BP_FRU_ADDR       (0x50)

//BP Mux
#define MUX_REG           (0x00)
#define BP_MUX1_MAX_PORT  (3)
#define BP_MUX2_MAX_PORT  (2)
#define MUX_ENABLE_PORT0  (0x01)
#define MUX_ENABLE_PORT1  (0x02)
#define MUX_ENABLE_PORT2  (0x04)
#define MUX_ENABLE_PORT3  (0x08)
#define BP_MUX0_PORT      (3)
#define BP_MUX2_PORT_0    (0)
#define BP_MUX2_PORT_1    (1)

//BP PSOC Control Reg
#define CTL_MAX_REG              (0x20)
#define CTL_REG_CFG_DISABLE      (0x14)
#define CTL_REG_CFG_VAL          (0x0D)
#define CTL_REG_CFG_ENABLE       (0x0E)
#define CTL_REG_SLOT_ID          (0x12)
#define CTL_REG_BAY_ID           (0x13)
#define CTL_REG_BP_NUM           (0x16)
#define CTL_REG_NUM_OF_SLOT      (0x17)
#define CTL_REG_FIRST_SLOT_NUM   (0x18)
#define CTL_REG_HFC_ID           (0x19)
#define CTL_REG_MGMT_PROTOCOL    (0x1A)
//CTL Reg default values
#define BP_CFG_DISABLE         (0xFF)
#define BP_CFG_ENABLE          (0xBE)
#define BP_CFG_VAL             (0x02)
#define BP_BAY_ID              (0x04)
#define BP_SLOT_ID             (0x40 | BP_BAY_ID)
#define BP_NUM                 (0x01)
#define BP_NUM_OF_SLOT         (0x04)
#define BP_FIRST_SLOT_NUM      (0x00)
#define BP_HFC_ID              (0x01)
#define BP_MGMT_PROTOCOL       (0x23)

//MISC
#define FILEPATHSIZE        (64)

// Return Code
#define SUCCESS             (0)
#define FAILURE             (-1)
#define BP_ERR_OPEN         (0x80)
#define BP_ERR_OPEN_I2C     (0x81)


// Add form Lenovo
//BP PSoC count
#define BP_TOTAL_SEP_0                                          	0
#define BP_TOTAL_SEP_1                                          	1
#define BP_TOTAL_SEP_2                                          	2
#define BP_TOTAL_SEP_3                                          	3

//BP BAY count
#define BP_TOTAL_BAY_2                                          	2
#define BP_TOTAL_BAY_4                                          	4
#define BP_TOTAL_BAY_6                                          	6
#define BP_TOTAL_BAY_8                                          	8
#define BP_TOTAL_BAY_10                                         	10
#define BP_TOTAL_BAY_12                                         	12

//BP group ID
#define BP_Group_ID_4                                           	4
#define BP_Group_ID_8                                           	8

//BP type ID
#define BP_TYPE_SAS_SATA                                        	0x01
#define BP_TYPE_ANYBAY                                          	0x02
#define BP_TYPE_NVME                                            	0x03
#define BP_TYPE_EDSFF                                           	0x04
#define BP_TOTAL_CONNECTOR                                      	8

//BP unique ID
#define BP_ID_NONE                                                  0xC0
#define BP_ID_2U_2_5_Anybay_8_Bay                                   0xC1

//BP auto configuration offset
#define BP_CONTROL_REGISTER_GROUP_ID                            	0x0D
#define BP_CONTROL_REGISTER_SLOT_ID                            		0x12
#define BP_CONTROL_REGISTER_BAY_ID                              	0x13
#define BP_CONTROL_REGISTER_VMD_CONFIGURATION                      	0x15
#define BP_CONTROL_REGISTER_BACKPLANE_INFO                    	  	0x16
#define BP_CONTROL_REGISTER_NUM_OF_SLOTS                        	0x17
#define BP_CONTROL_REGISTER_START_SLOT_NUM                      	0x18
#define BP_CONTROL_REGISTER_START_HFC_IDENTITY                  	0x19
#define BP_CONTROL_REGISTER_SYSTEM_MGMT_PROTOCOL                	0x1A
#define BP_CONTROL_REGISTER_AUTO_CONFIG_ENABLE                  	0x0E
#define BP_CONTROL_REGISTER_YELLOW_LED_SLOT_0_1                  	0x22
#define BP_CONTROL_REGISTER_PGOOD                  	                0x46

//BP PSoC relative reg
#define BP_SLAVE_ADDR_SEP_NVME_MUX                              	0x73            /* 8-bit address: 0xE6 */
#define BP_SLAVE_ADDR_SEP_STATUS_REG                            	0x20            /* 8-bit address: 0x40 */
#define BP_SLAVE_ADDR_SEP_CONTROL_REG                           	0x60            /* 8-bit address: 0xC0 */

//BP FRU
#define SYS_EEPROM_PATH_LENGTH										64
#define BP_FRU_BOARD_PRODUCT_OFFSET                            		0x16
#define BP_FRU_BOARD_PRODUCT_SIZE                              		40


#define BP_SYSTEM_TYPE_INTEL_GP                                 	0x00
#define BP_SYSTEM_TYPE_AMD_GP                 	                  	0x20
#define BP_SYSTEM_TYPE_INTEL_HS                                 	0x40
#define BP_SYSTEM_TYPE_AMD_HS                                   	0x60

#define BP_MANAGEMENT_PROTOCOL_SGPIO_I2CHP                      	0x03
#define BP_MANAGEMENT_PROTOCOL_I2CHP_UBM                        	0x06
#define BP_MANAGEMENT_PROTOCOL_SGPIO_I2CHP_UBM                  	0x07


#define PREFIX_BPBUS "/dev/i2c-2%d"
#define BP1_FRU_PATH "/sys/bus/i2c/devices/250-0050/eeprom"
#define BP2_FRU_PATH "/sys/bus/i2c/devices/251-0050/eeprom"
#define BP3_FRU_PATH "/sys/bus/i2c/devices/252-0050/eeprom"



typedef struct
{
	char  BP_Name[BP_FRU_BOARD_PRODUCT_SIZE];
	uint8_t BP_ID;
	uint8_t BP_Total_SEP;
	uint8_t BP_Total_Bay;
	uint8_t BP_Type;
	uint8_t BP_Group_ID;
    uint8_t BP_HFC[2];
} BP_Info;

typedef struct
{
	uint8_t BP_Connector_Offset;
	const char *BP_EEPROM;
	uint8_t Disk_Start_Index;
} BP_Config;


BP_Info   BP_Present_List[BP_TOTAL_CONNECTOR];
//Disk_Info BP_Disk_Info[BP_TOTAL_MONITOR_DISK];
//bool      EEPROM_VMD_Status[EEPROM_VMD_TOTAL_BIT];


#endif

