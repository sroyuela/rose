#ifndef UA_HDR
#define UA_HDR

     //Common structures
       #include "analysis/UACommon.h"
       #include "UGraphFunc.h"

     //Base class (exports visitor etc.)
       #include "analysis/UABase.h"
       //Readers & writer analysis
         #include "analysis/UAReadWriteAnal.h"
         //Basic block dependency analysis
           #include "analysis/UADependAnal.h"
         //Dataflow analysis
           #include "analysis/UADataFlowAnal.h"
           //Liveness analysis
             #include "analysis/dataflow/UADFLiveAnal.h"
           //Reaching def analysis
             #include "analysis/dataflow/UADFReachDefAnal.h"

     //Structural analysis
       #include "analysis/UAStructAnal.h"

#endif
