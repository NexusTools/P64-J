#include "stdafx.h"

CN64System    * _System = NULL;
CN64System    * _BaseSystem = NULL;
CN64System    * _SyncSystem = NULL;
CRecompiler   * _Recompiler = NULL;
CMipsMemory   * _MMU = NULL; //Memory of the n64 
CTLB          * _TLB = NULL; //TLB Unit
CRegisters    * _Reg = NULL; //Current Register Set attacted to the _MMU
CNotification * _Notify = NULL;   
CPlugins      * _Plugins = NULL;
CN64Rom       * _Rom = NULL;      //The current rom that this system is executing.. it can only execute one file at the time
CAudio        * _Audio = NULL;
CMemoryLabel  * _Labels = NULL;
CSystemTimer  * _SystemTimer = NULL;
CTransVaddr   * _TransVaddr = NULL;
CSystemEvents * _SystemEvents = NULL;

int * _NextTimer;


