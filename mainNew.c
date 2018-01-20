#include "led.h"
#include "key.h"
#include "delay.h"
#include "usart.h"
#include "lcd.h"
#include "sys.h"

#include "includes.h"
#include "24cxx.h"
#include "myiic.h"

void GPIO_Configuration(void);
unsigned long Read_HX711(void);
void testGPIO(void);
//void BLC_task(void);

// ====== GLOBAL VARIABLES ======
u32 zero;
u32 weight1;

// ====== ======
//START 任务
//设置任务优先级
#define START_TASK_PRIO      		10  //开始任务的优先级设置为最低
//设置任务堆栈大小
#define START_STK_SIZE  				64
//任务堆栈
OS_STK START_TASK_STK[START_STK_SIZE];
//任务函数
void start_task(void *pdata);

//KEY任务
//设置任务优先级
#define KEY_TASK_PRIO       			7
//设置任务堆栈大小
#define KEY_STK_SIZE  		    		64
//任务堆栈
OS_STK KEY_TASK_STK[KEY_STK_SIZE];
//任务函数
void key_task(void *pdata);

// Weight 任务
//设置任务优先级
#define BLC_TASK_PRIO       			5
//设置任务堆栈大小
#define BLC_STK_SIZE  		    		64
//任务堆栈
OS_STK BLC_TASK_STK[BLC_STK_SIZE];
//任务函数
void BLC_task(void *pdata);

//LCD 任务
//设置任务优先级
#define lcd_TASK_PRIO       			6
//设置任务堆栈大小
#define lcd_STK_SIZE  		    		64
//任务堆栈
OS_STK lcd_TASK_STK[BLC_STK_SIZE];
//任务函数
void lcd_task(void *pdata);


int main(void) {
  u8 lcd_id[12];

  // initialization
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
  delay_init();
  uart_init(9600);
  LED_Init();
  KEY_Init();
  LCD_Init();
  GPIO_Configuration();


  sprintf((char*)lcd_id,"LCD ID:%04X",lcddev.id);

  // ucos task
  OSInit();
  OSTaskCreate(start_task,(void *)0,(OS_STK *)&START_TASK_STK[START_STK_SIZE-1],START_TASK_PRIO );
  OSStart();

  //testGPIO();
  //  BLC_task();

  LED0 = 1;
  LED1 = 1;
 
}

void key_task(void *pdata) {
  // 标定功能
  u8 t=0;
  while(1)
  {
    t=KEY_Scan(0);
    switch(t)
    {
      // clear
      case KEY0_PRES:
        zero = 0; // ???
        LED1 = 0;
        printf("key0 pressed...");
        break;
      // set zero
      case KEY1_PRES:
        zero = weight1;
        LED1 = 1;
        printf("key1 pressed...zero is %u", zero);
        break;
      default:
        delay_ms(50);
    }
  }
}

// 称重
void BLC_task(void *pdata) {
//void BLC_task(void) {
  float weigh2;
  zero = 0;
  while(1) {
    delay_ms(1000);
    printf("...Start main logic...\n");

    weigh2 = Read_HX711();
    weight1 = weigh2/429.5 - zero;
    printf("%d\n",weight1);

    LED0 = !LED0;
  }
}

// TFLCD 进程
void lcd_task(void *pdata){
//change your shown string here
  while(1) {
    delay_ms(1000);
    POINT_COLOR=RED;
    //printf("lcd task")；
    LCD_ShowString(30,40,200,24,24,"Mini STM32 ^_^");
    LCD_ShowNum(30,70,weight1, 4, 16);
    LCD_ShowString(30,90,200,16,16,"CSE@SUSTech");
    //LCD_ShowString(30,110,200,16,16,lcd_id);
    LCD_ShowString(30,130,200,12,12,"2018/1/19");
  }
}

void start_task(void *pdata)
{
  OS_CPU_SR cpu_sr=0;
	pdata = pdata;
  OS_ENTER_CRITICAL();
  OSTaskCreate(key_task,(void *)0,(OS_STK*)&KEY_TASK_STK[KEY_STK_SIZE-1],KEY_TASK_PRIO);
 	OSTaskCreate(BLC_task,(void *)0,(OS_STK*)&BLC_TASK_STK[BLC_STK_SIZE-1],BLC_TASK_PRIO);
  OSTaskCreate(lcd_task,(void *)0,(OS_STK*)&lcd_TASK_STK[lcd_STK_SIZE-1],lcd_TASK_PRIO);
	OSTaskSuspend(START_TASK_PRIO);
	OS_EXIT_CRITICAL();
}

void GPIO_Configuration(void)
{
  // CLK:PB12   Data:PB11
  GPIO_InitTypeDef  GPIO_InitStructure;                //声明一个结构体变量

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

  // DOUT
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;         //选择 PB.11
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;         //浮空输入
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;  //IO口速度为50MHz
  GPIO_Init(GPIOB,&GPIO_InitStructure);                                 //初始化PB11

  // SCK
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;         //管脚频率为50MHZ
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOB,&GPIO_InitStructure);                                 //初始化PB12                               //初始化PB5
}

unsigned long Read_HX711(void)  //读HX711芯片输出的数据。
{
  unsigned long val = 8388608;
  unsigned char i = 0;
  unsigned long tmp = 0;

  GPIO_SetBits(GPIOB,GPIO_Pin_11);    //DOUT=1
  GPIO_ResetBits(GPIOB,GPIO_Pin_12);    //SCK=0
  while(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_11));   //等待DOUT=0
  printf("Start receiving data bits...");
  delay_us(1);
  for(i=0;i<24;i++)
  {
    GPIO_SetBits(GPIOB,GPIO_Pin_12);    //SCK=1
    delay_us(1);
    GPIO_ResetBits(GPIOB,GPIO_Pin_12);    //SCK=0
    if(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_11))   //DOUT=1;
      tmp = tmp | val;
    val=val>>1;
    delay_ms(1);
  }
  GPIO_SetBits(GPIOB,GPIO_Pin_12);
  delay_us(1);
  GPIO_ResetBits(GPIOB,GPIO_Pin_12);
  delay_us(1);
  return tmp;
}

/*
 * test: GPIO for balance
 */
void testGPIO(void) {
  // one
   while(1) {
     GPIO_SetBits(GPIOB,GPIO_Pin_11);

     if(GPIO_ReadOutputDataBit(GPIOB,GPIO_Pin_11)) printf("b.12: on\n");

     GPIO_ResetBits(GPIOB,GPIO_Pin_11);

     if(!GPIO_ReadOutputDataBit(GPIOB,GPIO_Pin_11)) printf("b.12: off\n");

     delay_ms(500);
     LED0 = !LED0;
   }

// two
   while(1) {
     delay_ms(500);
     if(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_11)) printf("b.11: on\n");
     else printf("b.11: off\n");
   }
 }
