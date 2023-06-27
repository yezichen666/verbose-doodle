#include "../SYSTEM/sys/sys.h"
#include "../SYSTEM/delay/delay.h"


static uint16_t  g_fac_us = 0;      /* us延时函数*/

/* 如果SYS_SUPPORT_OS被定义了，说明要支持OS了（不限于UCOS） */
#if SYS_SUPPORT_OS

#include "includes.h"

static uint16_t g_fac_ms = 0;

#ifdef  OS_CRITICAL_METHOD                      
#define delay_osrunning     OSRunning          
#define delay_ostickspersec OS_TICKS_PER_SEC   
#define delay_osintnesting  OSIntNesting      
#endif


#ifdef  CPU_CFG_CRITICAL_METHOD                
#define delay_osrunning     OSRunning          
#define delay_ostickspersec OSCfg_TickRate_Hz  
#define delay_osintnesting  OSIntNestingCtr     
#endif

static void delay_osschedlock(void)
{
#ifdef CPU_CFG_CRITICAL_METHOD  
    OS_ERR err;
    OSSchedLock(&err);          
#else                          
    OSSchedLock();              
#endif
}

static void delay_osschedunlock(void)
{
#ifdef CPU_CFG_CRITICAL_METHOD  
    OS_ERR err;
    OSSchedUnlock(&err);        
#else                          
    OSSchedUnlock();           
#endif
}

static void delay_ostimedly(uint32_t ticks)
{
#ifdef CPU_CFG_CRITICAL_METHOD
    OS_ERR err;
    OSTimeDly(ticks, OS_OPT_TIME_PERIODIC, &err);   
#else
    OSTimeDly(ticks);                              
#endif
}

void SysTick_Handler(void)
{
    if (delay_osrunning == 1)   
    {
        OSIntEnter();          
        OSTimeTick();          
        OSIntExit();           
    HAL_IncTick();
}
#endif

/**
 * @brief       初始化延时函数
 * @param       sysclk: 系统时钟频率，即为CPU频率(HCLK)
 * @retval      无
 */
void delay_init(uint16_t sysclk)
{
#if SYS_SUPPORT_OS /*如果需要支持OS. */
    uint32_t reload;
#endif
    SysTick->CTRL = 0;                                          /* 清Systick状态，以便下一步重设，如果这里开了中断会关闭其中断 */
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK_DIV8);   /* SYSTICK使用内核时钟源8分频,因systick的计数器最大值只有2^24 */

    g_fac_us = sysclk / 8;                                      /* 不论是否使用OS,g_fac_us都需要使用,作为1us的基础时基  */
#if SYS_SUPPORT_OS                                              /*如果需要支持OS */
    reload = sysclk / 8;                                        
    reload *= 1000000 / delay_ostickspersec;                   
                                                                
                                                                 
    g_fac_ms = 1000 / delay_ostickspersec;                      
    SysTick->CTRL |= 1 << 1;                                   
    SysTick->LOAD = reload;                                    
    SysTick->CTRL |= 1 << 0;                                   
#endif
}

#if SYS_SUPPORT_OS  

/**
 * @brief       ��ʱnus
 * @param       nus: Ҫ��ʱ��us��.
 * @note        nusȡֵ��Χ: 0 ~ 477218588(���ֵ��2^32 / g_fac_us @g_fac_us = 9)
 * @retval      ��
 */
void delay_us(uint32_t nus)
{
    uint32_t ticks;
    uint32_t told, tnow, tcnt = 0;
    uint32_t reload;
    reload = SysTick->LOAD;     /* LOAD��ֵ */
    ticks = nus * g_fac_us;     /* ��Ҫ�Ľ����� */
    delay_osschedlock();        /* ��ֹOS���ȣ���ֹ���us��ʱ */
    told = SysTick->VAL;        /* �ս���ʱ�ļ�����ֵ */

    while (1)
    {
        tnow = SysTick->VAL;

        if (tnow != told)
        {
            if (tnow < told)
            {
                tcnt += told - tnow;    /* ����ע��һ��SYSTICK��һ���ݼ��ļ������Ϳ�����. */
            }
            else
            {
                tcnt += reload - tnow + told;
            }

            told = tnow;

            if (tcnt >= ticks) break;   /* ʱ�䳬��/����Ҫ�ӳٵ�ʱ��,���˳�. */
        }
    }

    delay_osschedunlock();              /* �ָ�OS���� */
}

/**
 * @brief       ��ʱnms
 * @param       nms: Ҫ��ʱ��ms�� (0< nms <= 65535)
 * @retval      ��
 */
void delay_ms(uint16_t nms)
{
    if (delay_osrunning && delay_osintnesting == 0)     /* ���OS�Ѿ�������,���Ҳ������ж�����(�ж����治���������) */
    {
        if (nms >= g_fac_ms)                            /* ��ʱ��ʱ�����OS������ʱ������ */
        {
            delay_ostimedly(nms / g_fac_ms);            /* OS��ʱ */
        }

        nms %= g_fac_ms;                                /* OS�Ѿ��޷��ṩ��ôС����ʱ��,������ͨ��ʽ��ʱ */
    }

    delay_us((uint32_t)(nms * 1000));                   /* ��ͨ��ʽ��ʱ */
}

#else  /* 不使用OS时, 用以下代码 */

/**
 * @brief       延时nus
 * @param       nus: 要延时的us数.
 * @note        注意: nus的值,不要大于1864135us(最大值即2^24 / g_fac_us  @g_fac_us = 9)
 * @retval      无
 */
void delay_us(uint32_t nus)
{
    uint32_t temp;
    SysTick->LOAD = nus * g_fac_us; /*  时间加载 */
    SysTick->VAL = 0x00;            /* 清空计数器 */
    SysTick->CTRL |= 1 << 0 ;       /* 开始倒数 */

    do
    {
        temp = SysTick->CTRL;
    } while ((temp & 0x01) && !(temp & (1 << 16))); /*CTRL.ENABLE位必须为1, 并等待时间到达 */

    SysTick->CTRL &= ~(1 << 0) ;    /* 关闭SYSTICK */
    SysTick->VAL = 0X00;            /* 清空计数器 */
}

/**
 * @brief       延时nms
 * @param       nms: 要延时的ms数 (0< nms <= 65535)
 * @retval      无
 */
void delay_ms(uint16_t nms)
{
    uint32_t repeat = nms / 1000;   /*  这里用1000,是考虑到可能有超频应用
                                     *  比如128Mhz的时候, delay_us最大只能延时1048576us左右了
                                     */
    uint32_t remain = nms % 1000;

    while (repeat)
    {
        delay_us(1000 * 1000);      /* 利用delay_us 实现 1000ms 延时 */
        repeat--;
    }

    if (remain)
    {
        delay_us(remain * 1000);    /*利用delay_us, 把尾数延时(remain ms)给做了  */
    }
}

/**
  * @brief HAL库内部函数用到的延时
          HAL库的延时默认用Systick，如果我们没有开Systick的中断会导致调用这个延时后无法退出
  * @param Delay Delay 要延时的毫秒数
  * @retval None
  */
void HAL_Delay(uint32_t Delay)
{
     delay_ms(Delay);
}

#endif

































