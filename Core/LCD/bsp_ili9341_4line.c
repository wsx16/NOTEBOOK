/**
  ******************************************************************************
  * @file   bsp_ili9341_4line.c
  * @brief  2.8寸屏ILI9341驱动文件，采用4线SPI
  * 
  ******************************************************************************
  */
#include "bsp_ili9341_4line.h"
#include "spi.h"
//
static uint8_t DFT_SCAN_DIR; //扫描方向 
//管理ILI9341重要参数
static _ILI9341_dev ILI9341dev;		  
/*
**********************************************************************
* @fun     :ILI9341_WR_REG 
* @brief   :写寄存器函数
* @param   :REG:寄存器值
* @return  :None 
**********************************************************************
*/
inline void ILI9341_WR_REG(uint8_t REG)
{ 
	HAL_GPIO_WritePin(LCD_DCX_GPIO_Port, LCD_DCX_Pin, GPIO_PIN_RESET);
	
	HAL_SPI_Transmit(&hspi1,&REG,1, 1);  //不读取从机返回数据 	
	
	HAL_GPIO_WritePin(LCD_DCX_GPIO_Port, LCD_DCX_Pin, GPIO_PIN_SET);
}
/*
**********************************************************************
* @fun     :ILI9341_WR_DATA 
* @brief   :写ILI9341数据
* @param   :DATA:要写入的值
* @return  :None 
**********************************************************************
*/
inline void ILI9341_WR_DATA(uint8_t DATA)
{										    	   
	HAL_SPI_Transmit(&hspi1,&DATA,1, 1);  //不读取从机返回数据 		
}		
/*
**********************************************************************
* @fun     :ILI9341_WriteReg 
* @brief   :ILI9341_Reg:寄存器地址，ILI9341_RegValue:要写入的数据
* @param   :
* @return  :None 
**********************************************************************
*/
inline void ILI9341_WriteReg(uint8_t ILI9341_Reg, uint8_t ILI9341_RegValue)
{	
	ILI9341_WR_REG(ILI9341_Reg);
	ILI9341_WR_DATA(ILI9341_RegValue);
}	
/*
**********************************************************************
* @fun     :ILI9341_WriteRAM_Prepare 
* @brief   :开始写GRAM
* @param   :
* @return  :None 
**********************************************************************
*/
inline void ILI9341_WriteRAM_Prepare(void)
{
 	ILI9341_WR_REG(ILI9341dev.wramcmd);
}	 
/*
**********************************************************************
* @fun     :ILI9341_WriteRAM 
* @brief   :ILI9341写GRAM，SPI数据写入方式不同，功能同ILI9341_WriteRAM_Prepare
* @param   :
* @return  :None 
**********************************************************************
*/
inline void ILI9341_WriteRAM(uint16_t DAT)
{							    
	uint8_t TempBufferD[2] = {DAT >> 8, DAT};
	HAL_SPI_Transmit(&hspi1, TempBufferD, 2, 1);
	
	HAL_SPI_Transmit(&hspi1, TempBufferD, 2, 1); 		
}
/*
**********************************************************************
* @fun     :ILI9341_SetCursor 
* @brief   :设置光标位置
* @param   :Xpos:横坐标，Ypos:纵坐标
* @return  :None 
**********************************************************************
*/
inline void ILI9341_SetCursor(uint16_t Xpos, uint16_t Ypos)
{	 
	uint8_t TempBufferX[2] = {Xpos >> 8, Xpos & 0XFF};
	uint8_t TempBufferY[2] = {Ypos >> 8, Ypos & 0XFF};
	//
	ILI9341_WR_REG(ILI9341dev.setxcmd); 
	//
	HAL_SPI_Transmit(&hspi1, TempBufferX, 2, 1);			
	//	
	ILI9341_WR_REG(ILI9341dev.setycmd);
	//
	HAL_SPI_Transmit(&hspi1, TempBufferY, 2, 1);		
} 
/*
**********************************************************************
* @fun     :ILI9341_SetArea 
* @brief   :设置显示区域
* @param   :x0/x1:横坐标，y0/y1:纵坐标
* @return  :None 
**********************************************************************
*/
static uint16_t old_x0=0xFFFF, old_x1=0xFFFF, old_y0=0xFFFF, old_y1=0xFFFF;
//
inline void ILI9341_SetArea(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{	 
  uint8_t arguments[4];
  // Set columns, if changed
  if (x0 != old_x0 || x1 != old_x1)
  {
    arguments[0] = x0 >> 8;
    arguments[1] = x0 & 0xFF;
    arguments[2] = x1 >> 8;
    arguments[3] = x1 & 0xFF;
		//
		ILI9341_WR_REG(ILI9341dev.setxcmd); 
		HAL_SPI_Transmit(&hspi1, arguments, 4, 1);
		//
    old_x0 = x0;
    old_x1 = x1;
  }
  // Set rows, if changed
  if (y0 != old_y0 || y1 != old_y1)
  {
    arguments[0] = y0 >> 8;
    arguments[1] = y0 & 0xFF;
    arguments[2] = y1 >> 8;
    arguments[3] = y1 & 0xFF;
		//
		ILI9341_WR_REG(ILI9341dev.setycmd); 
		HAL_SPI_Transmit(&hspi1, arguments, 4, 1);
		//
    old_y0 = y0;
    old_y1 = y1;
  }	
} 
/*
**********************************************************************
* @fun     :ILI9341_Display_Dir 
* @brief   :设置ILI9341的自动扫描方向
							Memory Access Control (36h)
							This command defines read/write scanning direction of the frame memory.

							These 3 bits control the direction from the MPU to memory write/read.

							Bit  Symbol  Name  Description
							D7   MY  Row Address Order
							D6   MX  Column Address Order
							D5   MV  Row/Column Exchange
							D4   ML  Vertical Refresh Order  LCD vertical refresh direction control. 、

							D3   BGR RGB-BGR Order   Color selector switch control
										(0 = RGB color filter panel, 1 = BGR color filter panel )
							D2   MH  Horizontal Refresh ORDER  LCD horizontal refreshing direction control.
							D1   X   Reserved  Reserved
							D0   X   Reserved  Reserved
* @param   :
* @return  :None 
**********************************************************************
*/
void ILI9341_Display_Dir(Screen_ShowDIR ShowDIR)
{
	uint16_t regval=0x08; //RGB-BGR Order不能改变
	uint8_t dirreg=0;
	
	if(ShowDIR == SCAN_Vertical)	//竖屏
	{
		ILI9341dev.width=240;
		ILI9341dev.height=320;
 
		ILI9341dev.wramcmd=0X2C;
		ILI9341dev.setxcmd=0X2A;
		ILI9341dev.setycmd=0X2B;  
    DFT_SCAN_DIR=L2R_U2D;		
		
		switch(DFT_SCAN_DIR)
		{
			case L2R_U2D://从左到右,从上到下  //竖屏
				regval|=(0<<7)|(0<<6)|(0<<5); 
				break;
			case R2L_D2U://从右到左,从下到上   //竖屏
				regval|=(1<<7)|(1<<6)|(0<<5); 
				break;	 
		}
	}else //横屏
	{	  				
		ILI9341dev.width=320;
		ILI9341dev.height=240;
 
		ILI9341dev.wramcmd=0X2C;
		ILI9341dev.setxcmd=0X2A;
		ILI9341dev.setycmd=0X2B;  
		DFT_SCAN_DIR=D2U_L2R;	
		
		switch(DFT_SCAN_DIR)
		{
			case U2D_R2L://从上到下,从右到左  //横屏
				regval|=(0<<7)|(1<<6)|(1<<5); 
				break;
			case D2U_L2R://从下到上,从左到右  //横屏
				regval|=(1<<7)|(0<<6)|(1<<5); 
				break;
		}
	}
	dirreg=0X36; 
  regval|=0x00;	
	ILI9341_WriteReg(dirreg,regval);
	//设置光标在原点位置		
	ILI9341_WR_REG(ILI9341dev.setxcmd); 
	ILI9341_WR_DATA(0);ILI9341_WR_DATA(0);
	ILI9341_WR_DATA((ILI9341dev.width-1)>>8);ILI9341_WR_DATA((ILI9341dev.width-1)&0XFF);
	ILI9341_WR_REG(ILI9341dev.setycmd); 
	ILI9341_WR_DATA(0);ILI9341_WR_DATA(0);
	ILI9341_WR_DATA((ILI9341dev.height-1)>>8);ILI9341_WR_DATA((ILI9341dev.height-1)&0XFF);  	
}
/*
**********************************************************************
* @fun     :ILI9341_Init 
* @brief   :初始化ILI9341
* @param   :
* @return  :None 
**********************************************************************
*/
void ILI9341_Init(void)
{ 
  //ILI9341复位 
	HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
	HAL_Delay(1);
	HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);
	HAL_Delay(80);
	HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
	HAL_Delay(10);
	//************* Start Initial Sequence **********//
	ILI9341_WR_REG(0xCF);  
	ILI9341_WR_DATA(0x00); 
	ILI9341_WR_DATA(0xC1); 
	ILI9341_WR_DATA(0X30); 
	
	ILI9341_WR_REG(0xED);  
	ILI9341_WR_DATA(0x64); 
	ILI9341_WR_DATA(0x03); 
	ILI9341_WR_DATA(0X12); 
	ILI9341_WR_DATA(0X81); 
	
	ILI9341_WR_REG(0xE8);  
	ILI9341_WR_DATA(0x85); 
	ILI9341_WR_DATA(0x10); 
	ILI9341_WR_DATA(0x7A); 
	
	ILI9341_WR_REG(0xCB);  
	ILI9341_WR_DATA(0x39); 
	ILI9341_WR_DATA(0x2C); 
	ILI9341_WR_DATA(0x00); 
	ILI9341_WR_DATA(0x34); 
	ILI9341_WR_DATA(0x02); 
	
	ILI9341_WR_REG(0xF7);  
	ILI9341_WR_DATA(0x20); 
	
	ILI9341_WR_REG(0xEA);  
	ILI9341_WR_DATA(0x00); 
	ILI9341_WR_DATA(0x00); 
	
	ILI9341_WR_REG(0xC0);    //Power control 
	ILI9341_WR_DATA(0x1B);   //VRH[5:0] 
	
	ILI9341_WR_REG(0xC1);    //Power control 
	ILI9341_WR_DATA(0x01);   //SAP[2:0];BT[3:0] 
	
	ILI9341_WR_REG(0xC5);    //VCM control 
	ILI9341_WR_DATA(0x30); 	 //3F
	ILI9341_WR_DATA(0x30); 	 //3C
	
	ILI9341_WR_REG(0xC7);    //VCM control2 
	ILI9341_WR_DATA(0XB7); 
	
	ILI9341_WR_REG(0x36);    // Memory Access Control 
	ILI9341_WR_DATA(0x48);	 //RGB 
	
	ILI9341_WR_REG(0x3A);   
	ILI9341_WR_DATA(0x55); 
	
	ILI9341_WR_REG(0xB1);   
	ILI9341_WR_DATA(0x00);   
	ILI9341_WR_DATA(0x10);   //Frame Rate 119(Hz)
	
	ILI9341_WR_REG(0xB6);    // Display Function Control 
	ILI9341_WR_DATA(0x0A); 
	ILI9341_WR_DATA(0xA2); 
	
	ILI9341_WR_REG(0xF2);    // 3Gamma Function Disable 
	ILI9341_WR_DATA(0x00); 
	
	ILI9341_WR_REG(0x26);    //Gamma curve selected 
	ILI9341_WR_DATA(0x01); 
	
	ILI9341_WR_REG(0xE0);    //Set Gamma 
	ILI9341_WR_DATA(0x0F); 
	ILI9341_WR_DATA(0x2A); 
	ILI9341_WR_DATA(0x28); 
	ILI9341_WR_DATA(0x08); 
	ILI9341_WR_DATA(0x0E); 
	ILI9341_WR_DATA(0x08); 
	ILI9341_WR_DATA(0x54); 
	ILI9341_WR_DATA(0XA9); 
	ILI9341_WR_DATA(0x43); 
	ILI9341_WR_DATA(0x0A); 
	ILI9341_WR_DATA(0x0F); 
	ILI9341_WR_DATA(0x00); 
	ILI9341_WR_DATA(0x00); 
	ILI9341_WR_DATA(0x00); 
	ILI9341_WR_DATA(0x00); 
	
	ILI9341_WR_REG(0XE1);    //Set Gamma 
	ILI9341_WR_DATA(0x00); 
	ILI9341_WR_DATA(0x15); 
	ILI9341_WR_DATA(0x17); 
	ILI9341_WR_DATA(0x07); 
	ILI9341_WR_DATA(0x11); 
	ILI9341_WR_DATA(0x06); 
	ILI9341_WR_DATA(0x2B); 
	ILI9341_WR_DATA(0x56); 
	ILI9341_WR_DATA(0x3C); 
	ILI9341_WR_DATA(0x05); 
	ILI9341_WR_DATA(0x10); 
	ILI9341_WR_DATA(0x0F); 
	ILI9341_WR_DATA(0x3F); 
	ILI9341_WR_DATA(0x3F); 
	ILI9341_WR_DATA(0x0F); 
	
	ILI9341_WR_REG(0x2B); 
	ILI9341_WR_DATA(0x00);
	ILI9341_WR_DATA(0x00);
	ILI9341_WR_DATA(0x01);
	ILI9341_WR_DATA(0x3f);
	
	ILI9341_WR_REG(0x2A); 
	ILI9341_WR_DATA(0x00);
	ILI9341_WR_DATA(0x00);
	ILI9341_WR_DATA(0x00);
	ILI9341_WR_DATA(0xef);	
	
	ILI9341_WR_REG(0x35);	//Tearing Effect Line ON  
	ILI9341_WR_DATA(0x00);
	
	ILI9341_WR_REG(0x11);	//Sleep Out  
	ILI9341_WR_REG(0x21);	//Display Inversion ON 
	HAL_Delay(120);
	ILI9341_WR_REG(0x29); //display on	
  ILI9341_Display_Dir(SCAN_Vertical);	//竖屏显示
	//ILI9341_BL=1;					
	ILI9341_Clear(BLACK);
}  
/*
**********************************************************************
* @fun     :ILI9341_Clear 
* @brief   :清屏函数，color:要清屏的填充色
* @param   :
* @return  :None 
**********************************************************************
*/
void ILI9341_Clear(uint16_t color)
{
	uint8_t TempBufferD[2] = {color >> 8, color};
	//
	uint32_t index=0;      
	uint32_t totalpoint=ILI9341dev.width;
	totalpoint*=ILI9341dev.height; 	//得到总点数
	//
  ILI9341_SetCursor(0x00,0x00);	//设置光标位置
	ILI9341_WriteRAM_Prepare();     //开始写入GRAM	
	//	
	for(index=0;index < totalpoint;index++)
	{
		HAL_SPI_Transmit(&hspi1, TempBufferD, 2, 1); 		
	}
}
/*
**********************************************************************
* @fun     :_HW_DrawPoint 
* @brief   :uGUI函数调用，画点函数
* @param   :x,y:坐标，color:此点的颜色
* @return  :None 
**********************************************************************
*/
void _HW_DrawPoint(uint16_t x,uint16_t y,uint16_t color)
{	
	uint8_t TempBufferX[2] = {x >>8, x & 0XFF};
	uint8_t TempBufferY[2] = {y >>8, y & 0XFF};
	uint8_t TempBufferD[2] = {color >> 8, color};
	//
	ILI9341_WR_REG(ILI9341dev.setxcmd); 
	//
	HAL_SPI_Transmit(&hspi1, TempBufferX, 2, 1);		
	//
	ILI9341_WR_REG(ILI9341dev.setycmd); 
	//
	HAL_SPI_Transmit(&hspi1, TempBufferY, 2, 1);		
	//
	ILI9341_WR_REG(ILI9341dev.wramcmd); 
	//
	HAL_SPI_Transmit(&hspi1, TempBufferD, 2, 1);		
}	
/*
**********************************************************************
* @fun     :_HW_FillFrame 
* @brief   :在指定区域内填充单个颜色
* @param   :(sx,sy),(ex,ey):填充矩形对角坐标,区域大小为:(ex-sx+1)*(ey-sy+1),color:要填充的颜色
* @return  :None 
**********************************************************************
*/
void _HW_FillFrame(uint16_t sx,uint16_t sy,uint16_t ex,uint16_t ey,uint16_t color)
{  
	uint16_t i = 0,j = 0;
	uint16_t xlen = 0;
	xlen= ex - sx + 1;	   
	//
	uint8_t TempBuffer[2] = {color >> 8, color};	
	//数据写入屏幕
	for(i=sy;i<=ey;i++)
	{
	 	ILI9341_SetCursor(sx,i);      				//设置光标位置 
		ILI9341_WriteRAM_Prepare();     			//开始写入GRAM	  
		for(j=0;j<xlen;j++) 
		{
			HAL_SPI_Transmit(&hspi1, TempBuffer, 2, 1);//点设置颜色	
		}
	}
} 
/*
**********************************************************************
* @fun     :_HW_DrawLine 
* @brief   :画线
* @param   :_usX1,_usY1:起点坐标,_usX2,_usY2:终点坐标,_usColor:要填充的颜色
* @return  :None 
**********************************************************************
*/
void _HW_DrawLine(uint16_t _usX1,uint16_t _usY1,uint16_t _usX2,uint16_t _usY2,uint16_t _usColor)
{
	int32_t dx,dy;
	int32_t tx ,ty;
	int32_t inc1,inc2;
	int32_t d,iTag;
	int32_t x,y;
	/* 采用 Bresenham 算法，在2点间画一条直线 */
	_HW_DrawPoint(_usX1,_usY1,_usColor);
	/* 如果两点重合，结束后面的动作。*/
	if (_usX1==_usX2 && _usY1==_usY2) {return;}

	iTag =0;
	/* dx = abs ( _usX2 - _usX1 ); */
	if (_usX2 >=_usX1) {dx=_usX2-_usX1;}
	else {dx =_usX1-_usX2;}

	/* dy =abs (_usY2-_usY1); */
	if (_usY2 >=_usY1) {dy = _usY2-_usY1;}
	else {dy = _usY1-_usY2;}

	if (dx < dy)   /*如果dy为计长方向，则交换纵横坐标。*/
	{
		uint16_t temp;
		iTag = 1;
		temp = _usX1; _usX1 = _usY1; _usY1 = temp;
		temp = _usX2; _usX2 = _usY2; _usY2 = temp;
		temp = dx; dx = dy; dy = temp;
	}
	
	tx = _usX2 > _usX1 ? 1 : -1;    /* 确定是增1还是减1 */
	ty = _usY2 > _usY1 ? 1 : -1;
	x = _usX1;
	y = _usY1;
	inc1 = 2 * dy;
	inc2 = 2 * (dy - dx);
	d = inc1 - dx;
	while (x != _usX2)     /* 循环画点 */
	{
		if (d < 0) {d += inc1;}
		else {y += ty ; d += inc2 ;}
		
		if (iTag) { _HW_DrawPoint(y,x,_usColor);}
		else {_HW_DrawPoint(x,y,_usColor);}
		
		x += tx ;
	}	
}   
/*
**********************************************************************
* @fun     :LCD_DrawRect 
* @brief   :绘制水平放置的矩形
* @param   :
*			_usX,_usY: 矩形左上角的坐标
*			_usHeight : 矩形的高度
*			_usWidth  : 矩形的宽度
* @return  :None 
**********************************************************************
*/
void LCD_DrawRect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor)
{	
	_HW_DrawLine(_usX, _usY, _usX + _usWidth - 1, _usY, _usColor);	/* 顶 */
	_HW_DrawLine(_usX, _usY + _usHeight - 1, _usX + _usWidth - 1, _usY + _usHeight - 1, _usColor);	/* 底 */

	_HW_DrawLine(_usX, _usY, _usX, _usY + _usHeight - 1, _usColor);	/* 左 */
	_HW_DrawLine(_usX + _usWidth - 1, _usY, _usX + _usWidth - 1, _usY + _usHeight, _usColor);	/* 右 */	
}
/*
**********************************************************************
* @fun     :LCD_DrawCircle 
* @brief   :绘制一个圆，笔宽为1个像素
* @param   :
*			_usX,_usY  : 圆心的坐标
*			_usRadius  : 圆的半径
* @return  :None 
**********************************************************************
*/
void LCD_DrawCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint16_t _usColor)
{
	int32_t  D;			/* Decision Variable */
	uint32_t  CurX;		/* 当前 X 值 */
	uint32_t  CurY;		/* 当前 Y 值 */

	D = 3 - (_usRadius << 1);
	CurX = 0;
	CurY = _usRadius;

	while (CurX <= CurY)
	{
		_HW_DrawPoint(_usX + CurX, _usY + CurY, _usColor);
		_HW_DrawPoint(_usX + CurX, _usY - CurY, _usColor);
		_HW_DrawPoint(_usX - CurX, _usY + CurY, _usColor);
		_HW_DrawPoint(_usX - CurX, _usY - CurY, _usColor);
		_HW_DrawPoint(_usX + CurY, _usY + CurX, _usColor);
		_HW_DrawPoint(_usX + CurY, _usY - CurX, _usColor);
		_HW_DrawPoint(_usX - CurY, _usY + CurX, _usColor);
		_HW_DrawPoint(_usX - CurY, _usY - CurX, _usColor);

		if (D < 0)
		{
			D += (CurX << 2) + 6;
		}
		else
		{
			D += ((CurX - CurY) << 2) + 10;
			CurY--;
		}
		CurX++;
	}
}
/* 
 * 添加到 bsp_ili9341_4line.c 的末尾 
 * 功能：专门配合 LVGL 批量写入显存
 */
//void ILI9341_WriteBuffer(uint16_t *pData, uint32_t Size)
//{
//    // 1. 发送写内存指令 (0x2C)
//    // 你的结构体是 ILI9341dev，写内存指令是 wramcmd (通常是 0x2C)
//    ILI9341_WR_REG(ILI9341dev.wramcmd); 
//    
//    // 2. 批量发送数据
//    // SPI 是 8 位模式，传输长度 = 像素数 * 2 (因为 1个像素 = 16bit = 2字节)
//    HAL_SPI_Transmit(&hspi1, (uint8_t*)pData, Size * 2, 1000);
//}
#define LCD_WIDTH  320

static uint8_t tx_buf[LCD_WIDTH * 2];

void ILI9341_WriteBuffer(uint16_t *pData, uint32_t Size)
{
    ILI9341_WR_REG(ILI9341dev.wramcmd);

    while(Size > 0)
    {
        uint32_t line_pixels = (Size > LCD_WIDTH) ? LCD_WIDTH : Size;

        for(uint32_t i = 0; i < line_pixels; i++)
        {
            tx_buf[2*i]     = pData[i] >> 8;
            tx_buf[2*i + 1] = pData[i] & 0xFF;
        }

        HAL_SPI_Transmit(&hspi1, tx_buf, line_pixels * 2, 1000);

        pData += line_pixels;
        Size  -= line_pixels;
    }
}

