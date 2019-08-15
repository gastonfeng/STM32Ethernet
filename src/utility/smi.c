#if 0
#include "stm32_def.h"
#define MDIO_IN()                        \
    {                                    \
        GPIOA->MODER &= ~(3 << (2 * 2)); \
        GPIOA->MODER |= 0 << 2 * 2;      \
    } //PA2
#define MDIO_OUT()                       \
    {                                    \
        GPIOA->MODER &= ~(3 << (2 * 2)); \
        GPIOA->MODER |= 1 << 2 * 2;      \
    } //PA2

#define MDIO PAout(2) //SCL
#define MDC PCout(1)  //SDA
#define READ_MDIO PAin(2)

void SMI_Init(void)
{
    //配置PA2 PC1为推完输出，GPIO模拟SMI
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; //推完输出
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;   //GPIO_PuPd_UP GPIO_PuPd_NOPULL
    GPIO_Init(GPIOA, &GPIO_InitStructure);         //PA2 MDIO

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL; //GPIO_PuPd_UP GPIO_PuPd_NOPULL
    GPIO_Init(GPIOC, &GPIO_InitStructure);           //PC1 MDC ，不需要上拉
    MDIO = 1;
    MDC = 0;
}

u8 SMI_Read_One_Byte(void)
{
    u8 i, receive = 0;
    //    MDIO_IN();
    for (i = 0; i < 8; i++)
    {
        receive <<= 1;
        MDC = 0; //拉低时钟
        delay_us(1);
        MDC = 1; //上升沿传输数据
        delay_us(1);
        if (READ_MDIO)
            receive |= 0x01; //上升沿，高电平期间读数据
        MDC = 0;             //时钟恢复低电平
    }
    return receive;
}

u8 SMI_Read_2Bit(void)
{
    u8 i, receive = 0;
    //    MDIO_IN();
    for (i = 0; i < 2; i++)
    {
        receive <<= 1;
        MDC = 0; //拉低时钟
        delay_us(1);
        MDC = 1; //上升沿传输数据
        delay_us(1);
        if (READ_MDIO)
            receive |= 0x01; //上升沿，高电平期间读数据
        MDC = 0;             //恢复低电平
    }
    return receive;
}

void SMI_Write_One_Byte(u8 data)
{
    u8 i;
    //    MDIO_OUT();
    for (i = 0; i < 8; i++)
    {
        MDC = 0;
        MDIO = (data & 0x80) >> 7; //准备数据
        delay_us(1);
        MDC = 1;     //上升沿传输数据
        delay_us(1); //保持一段时间
        MDC = 0;     //恢复低电平
        data <<= 1;
    }
}

void SMI_Write_2Bit(u8 data)
{
    u8 i;
    MDIO_OUT();
    for (i = 0; i < 2; i++)
    {
        MDC = 0;
        MDIO = (data & 0x2) >> 1;
        delay_us(1);
        MDC = 1;     //上升沿传输数据
        delay_us(1); //保持一段时间
        MDC = 0;     //恢复低电平
        data <<= 1;
    }
}

#define START_OF_FRAME_2bit 0x01
#define READ_OP_CODE_2bit 0x2
#define WRITE_OP_CODE_2bit 0x1
#define SMI_OP_CODE_2bit 0x00
#define SMI_TA 0x02
void SMI_Write_Frame(u16 PHYAddress, u16 PHYReg, u16 PHYValue)
{
    u8 addr;

    addr = (PHYAddress & 0x7) << 5 | (PHYReg & 0x1F);

    MDIO_OUT();
    ////32 bit Preamble
    SMI_Write_One_Byte(0xFF);
    SMI_Write_One_Byte(0xFF);
    SMI_Write_One_Byte(0xFF);
    SMI_Write_One_Byte(0xFF);
    SMI_Write_One_Byte(0xFF);
    SMI_Write_One_Byte(0xFF);
    SMI_Write_One_Byte(0xFF);
    SMI_Write_One_Byte(0xFF);

    SMI_Write_2Bit(START_OF_FRAME_2bit);
    SMI_Write_2Bit(WRITE_OP_CODE_2bit);
    SMI_Write_2Bit(0x00); //dir: 0x0 is write
    SMI_Write_One_Byte(addr);
    SMI_Write_2Bit(SMI_TA);
    SMI_Write_One_Byte(0xFF);
    SMI_Write_One_Byte(PHYValue);
    MDIO_IN(); //恢复输入高阻态
}

u16 SMI_Read_Reg(u16 PHYAddress, u16 PHYReg)
{
    u8 addr;
    u16 data;

    addr = (PHYAddress & 0x7) << 5 | (PHYReg & 0x1F);

    ////32 bit Preamble
    MDIO_OUT();
    SMI_Write_One_Byte(0xFF);
    SMI_Write_One_Byte(0xFF);
    SMI_Write_One_Byte(0xFF);
    SMI_Write_One_Byte(0xFF);

    SMI_Write_2Bit(START_OF_FRAME_2bit);
    SMI_Write_2Bit(READ_OP_CODE_2bit);
    SMI_Write_2Bit(0x0); //PHYAD[4:3] decide dir: 0x2/0x3 is write
    SMI_Write_One_Byte(addr);

    MDIO_IN();
    data = SMI_Read_2Bit(); //SMI_TA should be Z0
    data = SMI_Read_One_Byte();
    data = data << 8;
    data = data | SMI_Read_One_Byte();
    return data;
}

u8 KSZ8863_Get_Reg_Value(u8 port, u8 reg_num)
{
    u16 reg;
    //        reg = ETH_ReadPHYRegister(port, reg_num); //从port1的1号寄存器中读取网络速度和双工模式

    reg = SMI_Read_Reg(port, reg_num);

    printf("port %d, reg 0x%x = 0x%x \r\n", port, reg_num, reg);
    return reg;
}

void KSZ8863_Print_Reg_Value(void)
{
    u8 i, port;
    for (port = 1; port < 4; port++)
    {
        for (i = 0; i < 6; i++)
        {
            KSZ8863_Get_Reg_Value(port, i);
            delay_ms(100);
        }
    }
}
#endif