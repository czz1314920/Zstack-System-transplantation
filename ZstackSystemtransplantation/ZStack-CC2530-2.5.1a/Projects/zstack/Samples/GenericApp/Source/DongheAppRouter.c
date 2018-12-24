
#include "DongheApp.h"
#include "OSAL_DongheRouter.h"
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
#include "DebugTrace.h"
#include "mt_uart.h"
#include "MT.h"



byte DhAppRouterManage_TaskID;   // Task ID for internal task/event processing
                          // This variable will be received when
                          // DhAppRouterManage_Init() is called.
devStates_t DhAppRouterManage_NwkState=DEV_INIT;   //devStates_t; //DEV_INIT


byte DhAppRouterManage_TransID=0;  // This is the unique message ID (counter)

afAddrType_t MySendtest_Single_DstAddr;
endPointDesc_t  MySendtest_epDesc;
uint8 *SampleAppPeriodicCounter = "ZZ";
uint8  SampleAppCounter=2;


/*********************************************************************
 * LOCAL FUNCTIONS
 */
void DhAppRouterManage_ProcessZDOStateChange( void );
void DhAppRouterManage_ProcessMSGData( afIncomingMSGPacket_t *msg );
void DhAppRouterManage_HandleKeys(byte keys );
void SampleApp_SerialCMD(mtOSALSerialData_t *cmdMsg);
void SampleApp_SendPeriodicMessage( void );
uint16 ReadAdcValue(uint8 ChannelNum,uint8 DecimationRate,uint8 RefVoltage);

/*********************************************************************
 * NETWORK LAYER CALLBACKS
 */

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      DhAppRouterManage_Init
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

const uint16 MySendtest_INClusterList[MySendtest_MAX_OUTCLUSTERS]=
{
  MySendtest_PERIODIC_CLUSTERID,
  MySendtest_GUANG_CLUSTERID,
  MySendtest_WENDU_CLUSTERID,
  MySendtest_SHIDU_CLUSTERID
};

const uint16 MySendtest_OUTClusterList[MySendtest_MAX_INCLUSTERS]=
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


void DongheAppRouter_Init(uint8 task_id)
{
  DhAppRouterManage_TaskID = task_id;
  MySendtest_Single_DstAddr.addrMode=AddrBroadcast;
  MySendtest_Single_DstAddr.endPoint=MySendtest_ENDPOINT;
  MySendtest_Single_DstAddr.addr.shortAddr=0x0000;


  //DhAppRouterManage_TransID = 0;
  MySendtest_epDesc.endPoint=MySendtest_ENDPOINT;
  MySendtest_epDesc.task_id=&DhAppRouterManage_TaskID;
  MySendtest_epDesc.simpleDesc=(SimpleDescriptionFormat_t *)&MySendtest_SimpleDesc ;
  MySendtest_epDesc.latencyReq=noLatencyReqs;


  afRegister(&MySendtest_epDesc);
  RegisterForKeys(DhAppRouterManage_TaskID);

  MT_UartInit();
  MT_UartRegisterTaskID(task_id);
  HalUARTWrite(0,"Router\n",7);
}

/*********************************************************************
 * @fn      DhAppRouterManage_ProcessEvent
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
uint16 DongheAppRouter_ProcessEvent( uint8 task_id, uint16 events )
{
  afIncomingMSGPacket_t *MSGpkt;
  unsigned char buff[8];
  uint16 temp;
  //afDataConfirm_t *afDataConfirm;

  // Data Confirmation message fields
  //byte sentEP;
  //ZStatus_t sentStatus;
  //byte sentTransID;       // This should match the value sent
  (void)task_id;  // Intentionally unreferenced parameter

  if ( events & SYS_EVENT_MSG )
  {
    MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( DhAppRouterManage_TaskID );
    while ( MSGpkt )
    {
      switch ( MSGpkt->hdr.event )
      {
        case KEY_CHANGE:
          DhAppRouterManage_HandleKeys(((keyChange_t *)MSGpkt)->keys );
          break;

        case AF_INCOMING_MSG_CMD:
          DhAppRouterManage_ProcessMSGData(MSGpkt);
          break;

        case ZDO_STATE_CHANGE:
          DhAppRouterManage_NwkState = (devStates_t)(MSGpkt->hdr.status);
          if (DhAppRouterManage_NwkState == DEV_ROUTER || DhAppRouterManage_NwkState == DEV_END_DEVICE)
          {
            APCFG = (0x1<<4);
            HalLedBlink(HAL_LED_1,5,50,1000);
            osal_start_timerEx( DhAppRouterManage_TaskID,MySendtest_DEVICEID,SAMPLEAPP_SEND_PERIODIC_MSG_TIMEOUT );
            //DhAppRouterManage_ProcessZDOStateChange();
          }else{
            //HalLedBlink(HAL_LED_2,5,50,1000);
            //HalLedSet (HAL_LED_2, HAL_LED_MODE_ON);
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
      MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( DhAppRouterManage_TaskID );
    }

    // return unprocessed events
    return (events ^ SYS_EVENT_MSG);
  }

  if(events && MySendtest_DEVICEID)
  {

    /*实现传感器采集
    *并发送数据
    */
    APCFG = (0x1<<4);           //P0.4为ADC口采集压力传感器的电路信号
    temp = ReadAdcValue(0x4,3,2);//P0.1采集光照度，12bit,AVDD5作为参考
    temp = (temp>>6);
    buff[0] = (uint8)(temp&0xff);
    buff[1] = (buff[0]>>4)&0xf;
    buff[2] =  buff[0]&0xf;
    if(buff[1] > 0x9)
        buff[1] = buff[1] - 0XA + 'A';
	else
	    buff[1] = buff[1] + '0';
    if(buff[2] > 0x9)
        buff[2] = buff[2] - 0XA + 'A';
	else
	    buff[2] = buff[2] + '0';
    SampleAppPeriodicCounter=&buff[1];
    SampleApp_SendPeriodicMessage();
    APCFG &= ~(0x1<<4);

    osal_start_timerEx( DhAppRouterManage_TaskID, MySendtest_DEVICEID,
        (SAMPLEAPP_SEND_PERIODIC_MSG_TIMEOUT + (osal_rand() & 0x00FF)) );

    return (events ^ MySendtest_DEVICEID);
  }

  return 0;
}

/*********************************************************************
 * Event Generation Functions
 */

/*********************************************************************
 * @fn      DhAppRouterManage_ProcessZDOMsgs()
 *
 * @brief   Process response messages
 *
 * @param   none
 *
 * @return  none
 */
void DhAppRouterManage_ProcessZDOStateChange()
{

}

/*********************************************************************
 * @fn      DhAppRouterManage_HandleKeys
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
void DhAppRouterManage_HandleKeys(byte keys)
{

  // Shift is used to make each button/switch dual purpose.
    if ( keys & HAL_KEY_SW_1 )
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

/*********************************************************************
 * LOCAL FUNCTIONS
 */

/*********************************************************************
 * @fn      DhAppRouterManage_MessageMSGCB
 *
 * @brief   Data message processor callback.  This function processes
 *          any incoming data - probably from other devices.  So, based
 *          on cluster ID, perform the intended action.
 *
 * @param   none
 *
 * @return  none
 */
void DhAppRouterManage_ProcessMSGData( afIncomingMSGPacket_t *msg )
{      //MySendtest_PERIODIC_CLUSTERID
  switch ( msg->clusterId )
  {
    case MySendtest_PERIODIC_CLUSTERID:
      HalUARTWrite(0,msg->cmd.Data,msg->cmd.DataLength);
      if(msg->cmd.Data[0]=='0')
      {
        HalLedSet (HAL_LED_1, HAL_LED_MODE_OFF);
      }else if(msg->cmd.Data[0]=='1'){
        HalLedSet (HAL_LED_1, HAL_LED_MODE_ON);
      }else if(msg->cmd.Data[0]=='2'){
        HalLedSet (HAL_LED_2, HAL_LED_MODE_OFF);
      }else if(msg->cmd.Data[0]=='3'){
        HalLedSet (HAL_LED_2, HAL_LED_MODE_ON);
      }else{};
      break;

    case MySendtest_WENDU_CLUSTERID:
      break;

    case MySendtest_GUANG_CLUSTERID:
      break;

    case MySendtest_SHIDU_CLUSTERID:
      break;

    default:
      break;

  }
}



void SampleApp_SendPeriodicMessage( void )
{                  //MySendtest_REWENDU_CLUSTERID协调器的
  if ( AF_DataRequest( &MySendtest_Single_DstAddr, &MySendtest_epDesc,
                       MySendtest_REWENDU_CLUSTERID,
                       SampleAppCounter,
                       (uint8*)&SampleAppPeriodicCounter,
                       &DhAppRouterManage_TransID,
                       AF_DISCV_ROUTE,
                       AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
  {
  }
  else
  {
    // Error occurred in request to send.
  }
}

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
}


uint16 ReadAdcValue(uint8 ChannelNum,uint8 DecimationRate,uint8 RefVoltage)
{
  uint16 AdValue;
  if(ChannelNum == 0xe){//片内温度到ADC_SOC
    TR0 = 1;
    ATEST = 1;
  }
  else{
    TR0 = 0;
    ATEST = 0;
  }

  ADCCON3 = ChannelNum&0xf;
  ADCCON3 = ADCCON3 | ((DecimationRate&0x3)<<4);
  ADCCON3 = ADCCON3 | ((RefVoltage&0x3)<<6);
  ADCCON1 = ADCCON1 | (0x3<<4);//ADCCON1.ST = 1时启动
  AdValue = ADCL; //清除EOC
  AdValue = ADCH;
  ADCCON1 = ADCCON1 | (0x1<<6);//启动转换
  while(!(ADCCON1&0x80));
  AdValue = ADCH;
  AdValue = (AdValue<<6) + (ADCL>>2);
  ADCCON1 =  ADCCON1 & 0x7f;
  return AdValue;
}