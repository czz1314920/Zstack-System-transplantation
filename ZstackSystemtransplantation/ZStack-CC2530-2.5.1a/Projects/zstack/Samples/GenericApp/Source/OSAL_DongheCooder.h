#ifndef OSAL_DONGHE_COODER_H
#define OSAL_DONGHE_COODER_H


#include "ZComDef.h"
#include "OSAL_Tasks.h"
#include "mac_api.h"
#include "nwk.h"
#include "hal_drivers.h"
#include "APS.h"
#include "ZDApp.h"

#endif


void DongheAppCooder_Init(byte task_id);
UINT16 DongheAppCooder_ProcessEvent(byte task_id,UINT16 event);

