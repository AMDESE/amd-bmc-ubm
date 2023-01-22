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

#endif

