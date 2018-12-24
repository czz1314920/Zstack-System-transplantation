#ifndef OSAL_DONGHE_ROUTER_H
#define OSAL_DONGHE_ROUTER_H


#include "ZComDef.h"
#include "OSAL_Tasks.h"
#include "mac_api.h"
#include "nwk.h"
#include "hal_drivers.h"
#include "APS.h"
#include "ZDApp.h"

//void DongheAppRouter_Init(byte task_id);
//UINT16 DongheAppRouter_ProcessEvent(byte task_id,UINT16 event);

#endif

void DongheAppRouter_Init(byte task_id);
UINT16 DongheAppRouter_ProcessEvent(byte task_id,UINT16 event);
