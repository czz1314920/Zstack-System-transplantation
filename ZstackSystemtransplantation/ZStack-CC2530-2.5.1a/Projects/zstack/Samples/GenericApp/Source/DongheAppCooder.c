
#include <string.h>
#include "DongheApp.h"
#include "OSAL_DongheCooder.h"
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_key.h"
#include "hal_uart.h"
#include "OnBoard.h"
#include "OSAL.h"
#include "AF.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"
#include "MT.h"
#include "mt_uart.h"

#define GENERICAPP_EVT 0x0001


byte DhAppCooderManage_TaskID;   // Task ID for internal task/event processing
                          // This variable will be received when
                          // DhAppCooderManage_Init() is called.
devStates_t DhAppCooderManage_NwkState=DEV_INIT;


byte DhAppCooderManage_TransID=0;  // This is the unique message ID (counter)

afAddrType_t MySendtest_Periodic_DstAddr;
#define SERIAL_APP_PORT          0
endPointDesc_t  MySendtest_epDesc;

uint8 SampleAppPeriodicCounter = 'z';


/*********************************************************************
 * LOCAL FUNCTIONS
 */
void DhAppCooderManage_ProcessZDOStateChange( void );
void DhAppCooderManage_ProcessMSGData( afIncomingMSGPacket_t *msg );
void DhAppCooderManage_HandleKeys(byte keys );
void SampleApp_SerialCMD(mtOSALSerialData_t *cmdMsg);
void SampleApp_SendPeriodicMessage( void );

/*********************************************************************
 * NETWORK LAYER CALLBACKS
 */

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      DhAppCooderManage_Init
 *
 * @brief   Initialization function for the Generic App Task.
 *          This is called during initialization and should contain
 *          any application specific initialization (ie. hardware
 *          initialization/setup, table initialization, power up
 *          notificaiton ... ).
 *
 * @param   task_id - the ID assigned by OSAL.  This ID should be
 *                    used to send messages and set timers.
 *
 * @return  none
 */



const uint16 MySendtest_OUTClusterList[MySendtest_MAX_OUTCLUSTERS]=
{
  MySendtest_PERIODIC_CLUSTERID,
  MySendtest_GUANG_CLUSTERID,
  MySendtest_WENDU_CLUSTERID,
  MySendtest_SHIDU_CLUSTERID
};

const uint16 MySendtest_INClusterList[MySendtest_MAX_INCLUSTERS]=
{
  MySendtest_SINGLE_CLUSTERID,
  MySendtest_REGUANG_CLUSTERID,
  MySendtest_REWENDU_CLUSTERID,
  MySendtest_RESHIDU_CLUSTERID
};

const SimpleDescriptionFormat_t MySendtest_SimpleDesc=
{
  MySendtest_ENDPOINT,
  MySendtest_PROFID,
  MySendtest_DEVICEID,
  MySendtest_DEVICE_VERSION,
  MySendtest_FLAGS,
  MySendtest_MAX_INCLUSTERS,
  (uint16*)MySendtest_INClusterList,
  MySendtest_MAX_OUTCLUSTERS,
  (uint16*)MySendtest_OUTClusterList
};

static void SerialApp_CallBack(uint8 port,uint8 event)
{
  uint8 sBuf[10]={0};
  uint16 nLen=0;

  if(event != HAL_UART_TX_EMPTY)
  {
    nLen = HalUARTRead(SERIAL_APP_PORT,sBuf,10);
    if(nLen>0)
    {
    }
  }
}

static void InitUart(void)
{
  halUARTCfg_t uartConfig;
  uartConfig.configured            = TRUE;
  uartConfig.baudRate              = HAL_UART_BR_57600;
  uartConfig.flowControl           = FALSE;
  uartConfig.flowControlThreshold  = 64;
  uartConfig.rx.maxBufSize         = 128;
  uartConfig.tx.maxBufSize         = 128;
  uartConfig.idleTimeout          = 6;
  uartConfig.intEnable             = TRUE;
  uartConfig.callBackFunc          = SerialApp_CallBack;

  HalUARTOpen(SERIAL_APP_PORT,&uartConfig);
}

void DongheAppCooder_Init( uint8 task_id )
{
  DhAppCooderManage_TaskID = task_id;
  MySendtest_Periodic_DstAddr.addrMode=afAddrBroadcast;
  MySendtest_Periodic_DstAddr.endPoint=MySendtest_ENDPOINT;
  MySendtest_Periodic_DstAddr.addr.shortAddr=0xffff;


  //DhAppCooderManage_TransID = 0;
  MySendtest_epDesc.endPoint=MySendtest_ENDPOINT;
  MySendtest_epDesc.task_id=&DhAppCooderManage_TaskID;
  MySendtest_epDesc.simpleDesc=(SimpleDescriptionFormat_t *)&MySendtest_SimpleDesc ;
  MySendtest_epDesc.latencyReq=noLatencyReqs;


  afRegister(&MySendtest_epDesc);
  RegisterForKeys(DhAppCooderManage_TaskID);

  InitUart();
  MT_UartRegisterTaskID(task_id);
  HalUARTWrite(0,"Hello Wrold!",12);
}

/*********************************************************************
 * @fn      DhAppCooderManage_ProcessEvent
 *
 * @brief   Generic Application Task event processor.  This function
 *          is called to process all events for the task.  Events
 *          include timers, messages and any other user defined events.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events - events to process.  This is a bit map and can
 *                   contain more than one event.
 *
 * @return  none
 */
uint16 DongheAppCooder_ProcessEvent( uint8 task_id, uint16 events )
{
  afIncomingMSGPacket_t *MSGpkt; //afIncomingMSGPacket_t
  //afDataConfirm_t *afDataConfirm;

  // Data Confirmation message fields
  //byte sentEP;
  //ZStatus_t sentStatus;
  //byte sentTransID;       // This should match the value sent
  (void)task_id;  // Intentionally unreferenced parameter

  if ( events & SYS_EVENT_MSG )
  {
    MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( DhAppCooderManage_TaskID );
    while ( MSGpkt )
    {
      switch ( MSGpkt->hdr.event )
      {
        case KEY_CHANGE:
          DhAppCooderManage_HandleKeys(((keyChange_t *)MSGpkt)->keys );
          break;

        case AF_INCOMING_MSG_CMD:
          DhAppCooderManage_ProcessMSGData(MSGpkt);
          break;

        case ZDO_STATE_CHANGE:
          DhAppCooderManage_NwkState = (devStates_t)(MSGpkt->hdr.status);
          if (DhAppCooderManage_NwkState == DEV_ZB_COORD)
          {
            HalLedSet (HAL_LED_1, HAL_LED_MODE_OFF);
            HalLedBlink(HAL_LED_2,5,50,1000);

            //DhAppCooderManage_ProcessZDOStateChange();
          }
          break;

         case CMD_SERIAL_MSG:
             SampleApp_SerialCMD((mtOSALSerialData_t *)MSGpkt);
             break;

        default:
          break;
      }

      // Release the memory
      osal_msg_deallocate( (uint8 *)MSGpkt );

      // Next
      MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( DhAppCooderManage_TaskID );
    }

    // return unprocessed events
    return (events ^ SYS_EVENT_MSG);
  }

  if(events & GENERICAPP_EVT)
  {
    HalLedBlink (HAL_LED_1,5,50,1000);
    return (events ^ GENERICAPP_EVT);
  }

  return 0;
}

/*********************************************************************
 * Event Generation Functions
 */

/*********************************************************************
 * @fn      DhAppCooderManage_ProcessZDOMsgs()
 *
 * @brief   Process response messages
 *
 * @param   none
 *
 * @return  none
 */
void DhAppCooderManage_ProcessZDOStateChange()
{
  HalLedBlink (HAL_LED_ALL,5,50,2000);
  osal_start_timerEx(DhAppCooderManage_TaskID,GENERICAPP_EVT,5000);
  HalLedSet(HAL_LED_1,HAL_LED_MODE_OFF);
  HalLedSet(HAL_LED_2,HAL_LED_MODE_OFF);
}



/*********************************************************************
 * @fn      DhAppCooderManage_HandleKeys
 *
 * @brief   Handles all key events for this device.
 *
 * @param   shift - true if in shift/alt.
 * @param   keys - bit field for key events. Valid entries:
 *                 HAL_KEY_SW_4
 *                 HAL_KEY_SW_3
 *                 HAL_KEY_SW_2
 *                 HAL_KEY_SW_1
 *
 * @return  none
 */
void DhAppCooderManage_HandleKeys(uint8 keys )
{

  // Shift is used to make each button/switch dual purpose.
    if ( keys & HAL_KEY_SW_1 )
    {
    }

    if ( keys & HAL_KEY_SW_2 )
    {
    }

    if ( keys & HAL_KEY_SW_3 )
    {
    }

    if ( keys & HAL_KEY_SW_4 )
    {
    }
    if ( keys & HAL_KEY_SW_5 )
    {
    }
    if ( keys & HAL_KEY_SW_6 )
    {
    }
}


/************************************************************************************************/
void SampleApp_SendPeriodicMessage( void )
{                  //MySendtest_REWENDU_CLUSTERID协调器的
  if ( AF_DataRequest( &MySendtest_Periodic_DstAddr, &MySendtest_epDesc,
                       MySendtest_PERIODIC_CLUSTERID,
                       1,
                       (uint8*)&SampleAppPeriodicCounter,
                       &DhAppCooderManage_TransID,
                       AF_DISCV_ROUTE,
                       AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
  {
  }
  else
  {
    // Error occurred in request to send.
  }
}
void DhAppCooderManage_ProcessMSGData( afIncomingMSGPacket_t *msg )
{   //接受路由器数据上去服务器
  switch ( msg->clusterId )
  {
    case MySendtest_REWENDU_CLUSTERID:
      HalUARTWrite(0,"12\r\n",4);
      HalUARTWrite(0,msg->cmd.Data,msg->cmd.DataLength);
      HalUARTWrite(0,"12\r\n",4);
      break;

    case MySendtest_SINGLE_CLUSTERID:
      HalUARTWrite(0,msg->cmd.Data,msg->cmd.DataLength);
      break;

    case MySendtest_REGUANG_CLUSTERID:
      HalUARTWrite(0,msg->cmd.Data,msg->cmd.DataLength);
      break;

    case MySendtest_RESHIDU_CLUSTERID:
      HalUARTWrite(0,msg->cmd.Data,msg->cmd.DataLength);
      break;

    default:
      break;
  }
}
/************************************************************************************************/

//接受来自服务器的数据
void SampleApp_SerialCMD(mtOSALSerialData_t *cmdMsg)
{
  uint8 i,len,*str=NULL; //len 有用数据长度
/********************************************************************/
  unsigned char seg7table[16] = {
    /* 0       1       2       3       4       5       6      7*/
    0xc0,   0xf9,   0xa4,   0xb0,   0x99,   0x92,   0x82,   0xf8,
    /* 8       9      A        B       C       D       E      F*/
    0x80,   0x90,   0x88,   0x83,   0xc6,   0xa1,   0x86,   0x8e };
    //P0DIR
  P0DIR |= 0x10;
  P1DIR = 0xff;
  P0 |= (0x1<<4);
  /********************************************************************/


    str=cmdMsg->msg; //指向数据开头
    len=*str; //msg 里的第 1 个字节代表后面的数据长度
/********打印出串口接收到的数据，用于提示*********/
    for(i=1;i<=len;i++)
    {

       if('0'<=*(str+i)&&*(str+i)<='9')
       {
         P1=seg7table[*(str+i)-'0'];
         HalUARTWrite(0,str+i,1 );
         HalLedSet (HAL_LED_2, HAL_LED_MODE_OFF);
         HalLedBlink(HAL_LED_1,5,50,1000);
       }
       else if('A'<=*(str+i)&&*(str+i)<='F' || 'a'<=*(str+i)&&*(str+i)<='f')
       {
         if('A'<=*(str+i)&&*(str+i)<='F')
          P1=seg7table[*(str+i)-'A'+10];
         else
           P1=seg7table[*(str+i)-'a'+10];
         HalUARTWrite(0,str+i,1 );
         HalLedSet (HAL_LED_1, HAL_LED_MODE_OFF);
         HalLedBlink(HAL_LED_2,5,50,1000);
       }
       else
       {
         HalUARTWrite(0,"error",5);
         P1=0xFF;
       }
    }
    //HalUARTWrite(0,"\n",1 );//换行


    P0DIR &= ~(0x10);
    P0 &= ~(0x1<<4);
    /*uint8 i,len,*str=NULL;
    str=cmdMsg->msg;
    len=*str;
    HalUARTWrite(0,str+1,1);
    if(len>=1){
      HalUARTWrite(0,str+1,1);
      switch(*(str+1))
      {
      case '0':
        SampleAppPeriodicCounter='0';
        SampleApp_SendPeriodicMessage();
        break;
      case '1':
        SampleAppPeriodicCounter='1';
        SampleApp_SendPeriodicMessage();
        break;
      case '2':
        SampleAppPeriodicCounter='2';
        SampleApp_SendPeriodicMessage();
        break;
      case '3':
        SampleAppPeriodicCounter='3';
        SampleApp_SendPeriodicMessage();
        break;
      default:
        break;
      }
    }  */
}