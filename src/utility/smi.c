#if 1
#include "stm32_def.h"
#include "core_debug.h"
void MDIO_IN()
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    //配置PA2 PC1为推完输出，GPIO模拟SMI
    GPIO_InitStruct.Pin = GPIO_PIN_2;
//    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP; //推完输出
    GPIO_InitStruct.Pull = GPIO_PULLUP;   //GPIO_PuPd_UP GPIO_PuPd_NOPULL
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
} //PA2
void MDIO_OUT()
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    //配置PA2 PC1为推完输出，GPIO模拟SMI
    GPIO_InitStruct.Pin = GPIO_PIN_2;
//    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP; //推完输出
    GPIO_InitStruct.Pull = GPIO_PULLUP;   //GPIO_PuPd_UP GPIO_PuPd_NOPULL
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
} //PA2

//#define MDIO PAout(2) //SCL
//#define MDC PCout(1)  //SDA
//#define READ_MDIO PAin(2)

void MDIO1()
{
    HAL_GPIO_WritePin(GPIOA,GPIO_PIN_2,1);
}
void MDIO0()
{
    HAL_GPIO_WritePin(GPIOA,GPIO_PIN_2,0);
}
void MDC0()
{
    HAL_GPIO_WritePin(GPIOC,GPIO_PIN_1,0);
}
void MDC1()
{
    HAL_GPIO_WritePin(GPIOC,GPIO_PIN_1,1);
}
int READ_MDIO()
{
    return HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_2)?1:0;
}
void delay_us(int t)
{
    volatile int i;
    for(i=1000; i>0; i--);
}
void SMI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    //配置PA2 PC1为推完输出，GPIO模拟SMI
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
//    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP; //推完输出
    GPIO_InitStruct.Pull = GPIO_PULLUP;   //GPIO_PuPd_UP GPIO_PuPd_NOPULL
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);         //PA2 MDIO

    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Pull = GPIO_NOPULL; //GPIO_PuPd_UP GPIO_PuPd_NOPULL
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);           //PC1 MDC ，不需要上拉
    MDIO1();
    MDC0();
}

uint8_t SMI_Read_One_Byte(void)
{
    uint8_t i, receive = 0;
    //    MDIO_IN();
    for (i = 0; i < 8; i++)
    {
        receive <<= 1;
        MDC0(); //拉低时钟
        delay_us(1);
        MDC1(); //上升沿传输数据
        delay_us(1);
        if (READ_MDIO())
            receive |= 0x01; //上升沿，高电平期间读数据
        MDC0();             //时钟恢复低电平
    }
    return receive;
}

uint8_t SMI_Read_2Bit(void)
{
    uint8_t i, receive = 0;
    //    MDIO_IN();
    for (i = 0; i < 2; i++)
    {
        receive <<= 1;
        MDC0(); //拉低时钟
        delay_us(1);
        MDC1(); //上升沿传输数据
        delay_us(1);
        if (READ_MDIO())
            receive |= 0x01; //上升沿，高电平期间读数据
        MDC0();             //恢复低电平
    }
    return receive;
}

void SMI_Write_One_Byte(uint8_t data)
{
    uint8_t i;
    //    MDIO_OUT();
    for (i = 0; i < 8; i++)
    {
        MDC0();
        ((data & 0x80) >> 7)?MDIO1():MDIO0(); //准备数据
        delay_us(1);
        MDC1();     //上升沿传输数据
        delay_us(1); //保持一段时间
        MDC0();     //恢复低电平
        data <<= 1;
    }
}

void SMI_Write_2Bit(uint8_t data)
{
    uint8_t i;
//    MDIO_OUT();
    for (i = 0; i < 2; i++)
    {
        MDC0();
        ((data & 0x2) >> 1)?MDIO1():MDIO0();
        delay_us(1);
        MDC1();     //上升沿传输数据
        delay_us(1); //保持一段时间
        MDC0();     //恢复低电平
        data <<= 1;
    }
}

#define START_OF_FRAME_2bit 0x01
#define READ_OP_CODE_2bit 0x2
#define WRITE_OP_CODE_2bit 0x1
#define SMI_OP_CODE_2bit 0x00
#define SMI_TA 0x02
void SMI_Write_Frame(uint16_t PHYAddress, uint16_t PHYReg, uint16_t PHYValue)
{
    uint8_t addr;

    addr = ((PHYAddress & 0x7) << 5 )| (PHYReg & 0x1F);

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

uint16_t SMI_Read_Reg(uint16_t PHYAddress, uint16_t PHYReg)
{
    uint8_t addr;
    uint16_t data;

    addr = ((PHYAddress & 0x7) << 5) | (PHYReg & 0x1F);

    ////32 bit Preamble
    MDIO_OUT();
    SMI_Write_One_Byte(0xFF);
    SMI_Write_One_Byte(0xFF);
    SMI_Write_One_Byte(0xFF);
    SMI_Write_One_Byte(0xFF);

    SMI_Write_2Bit(START_OF_FRAME_2bit);
    SMI_Write_2Bit(0x0); //PHYAD[4:3] decide dir: 0x2/0x3 is write
    SMI_Write_2Bit(READ_OP_CODE_2bit);
    SMI_Write_One_Byte(addr);

    MDIO_IN();
    data = SMI_Read_2Bit(); //SMI_TA should be Z0
    data = SMI_Read_One_Byte();
    data = data << 8;
    data = data | SMI_Read_One_Byte();
    return data;
}

uint8_t KSZ8863_Get_Reg_Value(uint8_t port, uint8_t reg_num)
{
    uint16_t reg;
    //        reg = ETH_ReadPHYRegister(port, reg_num); //从port1的1号寄存器中读取网络速度和双工模式
    char buf[32];
    reg = SMI_Read_Reg(port, reg_num);

    sprintf(buf,"port %d, reg 0x%x = 0x%x \r\n", port, reg_num, reg);
    core_debug(buf);
    return reg;
}

uint8_t ksz8863_reg(uint8_t addr)
{
    return SMI_Read_Reg((addr>>5)&0x7,addr&0x1f);
}
void KSZ8863_Print_Reg_Value(void)
{
    uint8_t i, port;
    for(i=0; i<10; i++)
    {
        char buf[32],reg;
        reg = ksz8863_reg(i);

        sprintf(buf," reg 0x%x = 0x%x \r\n",  i, reg);
        core_debug(buf);
    }
    for (port = 1; port < 4; port++)
    {
        for (i = 0; i < 6; i++)
        {
            KSZ8863_Get_Reg_Value(port, i);
            HAL_Delay(100);
        }
    }
}
#endif
