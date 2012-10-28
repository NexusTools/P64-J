#include "stdafx.h"

R4300iOp::Func * CInterpreterCPU::m_R4300i_Opcode = NULL;
DWORD CInterpreterCPU::m_CountPerOp = 2;

void ExecuteInterpreterOps (DWORD Cycles)
{
	_Notify->BreakPoint(__FILE__,__LINE__);
}

int DelaySlotEffectsCompare (DWORD PC, DWORD Reg1, DWORD Reg2) {
	OPCODE Command;

	if (!_MMU->LW_VAddr(PC + 4, Command.Hex)) {
		//DisplayError("Failed to load word 2");
		//ExitThread(0);
		return TRUE;
	}

	switch (Command.op) {
	case R4300i_SPECIAL:
		switch (Command.funct) {
		case R4300i_SPECIAL_SLL:
		case R4300i_SPECIAL_SRL:
		case R4300i_SPECIAL_SRA:
		case R4300i_SPECIAL_SLLV:
		case R4300i_SPECIAL_SRLV:
		case R4300i_SPECIAL_SRAV:
		case R4300i_SPECIAL_MFHI:
		case R4300i_SPECIAL_MTHI:
		case R4300i_SPECIAL_MFLO:
		case R4300i_SPECIAL_MTLO:
		case R4300i_SPECIAL_DSLLV:
		case R4300i_SPECIAL_DSRLV:
		case R4300i_SPECIAL_DSRAV:
		case R4300i_SPECIAL_ADD:
		case R4300i_SPECIAL_ADDU:
		case R4300i_SPECIAL_SUB:
		case R4300i_SPECIAL_SUBU:
		case R4300i_SPECIAL_AND:
		case R4300i_SPECIAL_OR:
		case R4300i_SPECIAL_XOR:
		case R4300i_SPECIAL_NOR:
		case R4300i_SPECIAL_SLT:
		case R4300i_SPECIAL_SLTU:
		case R4300i_SPECIAL_DADD:
		case R4300i_SPECIAL_DADDU:
		case R4300i_SPECIAL_DSUB:
		case R4300i_SPECIAL_DSUBU:
		case R4300i_SPECIAL_DSLL:
		case R4300i_SPECIAL_DSRL:
		case R4300i_SPECIAL_DSRA:
		case R4300i_SPECIAL_DSLL32:
		case R4300i_SPECIAL_DSRL32:
		case R4300i_SPECIAL_DSRA32:
			if (Command.rd == 0) { return FALSE; }
			if (Command.rd == Reg1) { return TRUE; }
			if (Command.rd == Reg2) { return TRUE; }
			break;
		case R4300i_SPECIAL_MULT:
		case R4300i_SPECIAL_MULTU:
		case R4300i_SPECIAL_DIV:
		case R4300i_SPECIAL_DIVU:
		case R4300i_SPECIAL_DMULT:
		case R4300i_SPECIAL_DMULTU:
		case R4300i_SPECIAL_DDIV:
		case R4300i_SPECIAL_DDIVU:
			break;
		default:
#ifndef EXTERNAL_RELEASE
			DisplayError("Does %s effect Delay slot at %X?",R4300iOpcodeName(Command.Hex,PC+4), PC);
#endif
			return TRUE;
		}
		break;
	case R4300i_CP0:
		switch (Command.rs) {
		case R4300i_COP0_MT: break;
		case R4300i_COP0_MF:
			if (Command.rt == 0) { return FALSE; }
			if (Command.rt == Reg1) { return TRUE; }
			if (Command.rt == Reg2) { return TRUE; }
			break;
		default:
			if ( (Command.rs & 0x10 ) != 0 ) {
				switch( Command.funct ) {
				case R4300i_COP0_CO_TLBR: break;
				case R4300i_COP0_CO_TLBWI: break;
				case R4300i_COP0_CO_TLBWR: break;
				case R4300i_COP0_CO_TLBP: break;
				default: 
#ifndef EXTERNAL_RELEASE
					DisplayError("Does %s effect Delay slot at %X?\n6",R4300iOpcodeName(Command.Hex,PC+4), PC);
#endif
					return TRUE;
				}
			} else {
#ifndef EXTERNAL_RELEASE
				DisplayError("Does %s effect Delay slot at %X?\n7",R4300iOpcodeName(Command.Hex,PC+4), PC);
#endif
				return TRUE;
			}
		}
		break;
	case R4300i_CP1:
		switch (Command.fmt) {
		case R4300i_COP1_MF:
			if (Command.rt == 0) { return FALSE; }
			if (Command.rt == Reg1) { return TRUE; }
			if (Command.rt == Reg2) { return TRUE; }
			break;
		case R4300i_COP1_CF: break;
		case R4300i_COP1_MT: break;
		case R4300i_COP1_CT: break;
		case R4300i_COP1_S: break;
		case R4300i_COP1_D: break;
		case R4300i_COP1_W: break;
		case R4300i_COP1_L: break;
#ifndef EXTERNAL_RELEASE
		default:
			DisplayError("Does %s effect Delay slot at %X?",R4300iOpcodeName(Command.Hex,PC+4), PC);
#endif
			return TRUE;
		}
		break;
	case R4300i_ANDI:
	case R4300i_ORI:
	case R4300i_XORI:
	case R4300i_LUI:
	case R4300i_ADDI:
	case R4300i_ADDIU:
	case R4300i_SLTI:
	case R4300i_SLTIU:
	case R4300i_DADDI:
	case R4300i_DADDIU:
	case R4300i_LB:
	case R4300i_LH:
	case R4300i_LW:
	case R4300i_LWL:
	case R4300i_LWR:
	case R4300i_LDL:
	case R4300i_LDR:
	case R4300i_LBU:
	case R4300i_LHU:
	case R4300i_LD:
	case R4300i_LWC1:
	case R4300i_LDC1:
		if (Command.rt == 0) { return FALSE; }
		if (Command.rt == Reg1) { return TRUE; }
		if (Command.rt == Reg2) { return TRUE; }
		break;
	case R4300i_CACHE: break;
	case R4300i_SB: break;
	case R4300i_SH: break;
	case R4300i_SW: break;
	case R4300i_SWR: break;
	case R4300i_SWL: break;
	case R4300i_SWC1: break;
	case R4300i_SDC1: break;
	case R4300i_SD: break;
	default:
#ifndef EXTERNAL_RELEASE
		DisplayError("Does %s effect Delay slot at %X?",R4300iOpcodeName(Command.Hex,PC+4), PC);
#endif
		return TRUE;
	}
	return FALSE;
}

void InPermLoop (void) {
	// *** Changed ***/
	//if (CPU_Type == CPU_SyncCores) { SyncRegisters.CP0[9] +=5; }

	/* Interrupts enabled */
	if (( _Reg->STATUS_REGISTER & STATUS_IE  ) == 0 ) { goto InterruptsDisabled; }
	if (( _Reg->STATUS_REGISTER & STATUS_EXL ) != 0 ) { goto InterruptsDisabled; }
	if (( _Reg->STATUS_REGISTER & STATUS_ERL ) != 0 ) { goto InterruptsDisabled; }
	if (( _Reg->STATUS_REGISTER & 0xFF00) == 0) { goto InterruptsDisabled; }
	
	/* check sound playing */
	_N64System->SyncToAudio();
	
	/* check RSP running */
	/* check RDP running */

	if (*_NextTimer > 0) {
		//_Reg->COUNT_REGISTER += *_Timer + 1;
		//if (CPU_Type == CPU_SyncCores) { SyncRegisters.CP0[9] += Timers.Timer + 1; }
		*_NextTimer = -1;
	}
	return;

InterruptsDisabled:
	if (_Plugins->Gfx()->UpdateScreen != NULL) { _Plugins->Gfx()->UpdateScreen(); }
	//CurrentFrame = 0;
	//CurrentPercent = 0;
	//DisplayFPS();
	DisplayError(GS(MSG_PERM_LOOP));
	_N64System->CloseCpu();

}

CInterpreterCPU::CInterpreterCPU () 
{

}

CInterpreterCPU::~CInterpreterCPU()
{
}

void CInterpreterCPU::BuildCPU (void )
{ 
	R4300iOp::m_TestTimer       = FALSE;
	R4300iOp::m_NextInstruction = NORMAL;
	R4300iOp::m_JumpToLocation  = 0;
	
	m_CountPerOp = _Settings->LoadDword(Game_CounterFactor);

	m_R4300i_Opcode = R4300iOp::BuildInterpreter();
	//m_R4300i_Opcode = R4300iOp32::BuildInterpreter();
}

void CInterpreterCPU::ExecuteCPU (void )
{ 	
	bool   & Done            = _N64System->m_EndEmulation;
	DWORD  & PROGRAM_COUNTER = *_PROGRAM_COUNTER;
	OPCODE & Opcode          = R4300iOp::m_Opcode;
	DWORD  & JumpToLocation  = R4300iOp::m_JumpToLocation;
	BOOL   & TestTimer       = R4300iOp::m_TestTimer;
	const BOOL & bDoSomething= _SystemEvents->DoSomething();
	int & NextTimer = *_NextTimer;
	
	BuildCPU();

	__try 
	{
		while(!Done)
		{
			if (_MMU->LW_VAddr(PROGRAM_COUNTER, Opcode.Hex)) 
			{
				/*if (PROGRAM_COUNTER > 0x80000300 && PROGRAM_COUNTER< 0x80380000)
				{
					WriteTraceF((TraceType)(TraceError | TraceNoHeader),"%X: %s",*_PROGRAM_COUNTER,R4300iOpcodeName(Opcode.Hex,*_PROGRAM_COUNTER));
					//WriteTraceF((TraceType)(TraceError | TraceNoHeader),"%X: %s t9: %08X v1: %08X",*_PROGRAM_COUNTER,R4300iOpcodeName(Opcode.Hex,*_PROGRAM_COUNTER),_GPR[0x19].UW[0],_GPR[0x03].UW[0]);
					//WriteTraceF((TraceType)(TraceError | TraceNoHeader),"%X: %d %d",*_PROGRAM_COUNTER,*_NextTimer,_SystemTimer->CurrentType());
				}*/
				NextTimer -= m_CountPerOp;
				m_R4300i_Opcode[ Opcode.op ]();
				
				switch (R4300iOp::m_NextInstruction)
				{
				case NORMAL: 
					PROGRAM_COUNTER += 4; 
					break;
				case DELAY_SLOT:
					R4300iOp::m_NextInstruction = JUMP;
					PROGRAM_COUNTER += 4; 
					break;
				case JUMP:
					{
						BOOL CheckTimer = (JumpToLocation < PROGRAM_COUNTER || TestTimer); 
						PROGRAM_COUNTER  = JumpToLocation;
						R4300iOp::m_NextInstruction = NORMAL;
						if (CheckTimer)
						{
							TestTimer = FALSE;
							if (NextTimer < 0) 
							{ 
								_SystemTimer->TimerDone();
							}
							if (bDoSomething)
							{
								_SystemEvents->ExecuteEvents();
							}
						}
					}
				}
			} else { 
				_Reg->DoTLBMiss(R4300iOp::m_NextInstruction == JUMP,PROGRAM_COUNTER);
				R4300iOp::m_NextInstruction = NORMAL;
			}
		}
	} __except( _MMU->MemoryFilter( GetExceptionCode(), GetExceptionInformation()) ) {
		DisplayError(GS(MSG_UNKNOWN_MEM_ACTION));
		ExitThread(0);
	}
}


void CInterpreterCPU::ExecuteOps ( int Cycles )
{
	bool   & Done            = _N64System->m_EndEmulation;
	DWORD  & PROGRAM_COUNTER = *_PROGRAM_COUNTER;
	OPCODE & Opcode          = R4300iOp::m_Opcode;
	DWORD  & JumpToLocation  = R4300iOp::m_JumpToLocation;
	BOOL   & TestTimer       = R4300iOp::m_TestTimer;
	const BOOL & DoSomething     = _SystemEvents->DoSomething();
	
	__try 
	{
		while(!Done)
		{
			if (Cycles <= 0) 
			{
				return;
			}
			
			if (_MMU->LW_VAddr(PROGRAM_COUNTER, Opcode.Hex)) 
			{
				/*if (PROGRAM_COUNTER > 0x80000300 && PROGRAM_COUNTER< 0x80380000)
				{
					WriteTraceF((TraceType)(TraceError | TraceNoHeader),"%X: %s",*_PROGRAM_COUNTER,R4300iOpcodeName(Opcode.Hex,*_PROGRAM_COUNTER));
					//WriteTraceF((TraceType)(TraceError | TraceNoHeader),"%X: %s t9: %08X v1: %08X",*_PROGRAM_COUNTER,R4300iOpcodeName(Opcode.Hex,*_PROGRAM_COUNTER),_GPR[0x19].UW[0],_GPR[0x03].UW[0]);
					//WriteTraceF((TraceType)(TraceError | TraceNoHeader),"%X: %d %d",*_PROGRAM_COUNTER,*_NextTimer,_SystemTimer->CurrentType());
				}*/
				/*if (PROGRAM_COUNTER > 0x80323000 && PROGRAM_COUNTER< 0x80380000)
				{
					WriteTraceF((TraceType)(TraceError | TraceNoHeader),"%X: %s",*_PROGRAM_COUNTER,R4300iOpcodeName(Opcode.Hex,*_PROGRAM_COUNTER));
					//WriteTraceF((TraceType)(TraceError | TraceNoHeader),"%X: %s t9: %08X v1: %08X",*_PROGRAM_COUNTER,R4300iOpcodeName(Opcode.Hex,*_PROGRAM_COUNTER),_GPR[0x19].UW[0],_GPR[0x03].UW[0]);
					//WriteTraceF((TraceType)(TraceError | TraceNoHeader),"%X: %d %d",*_PROGRAM_COUNTER,*_NextTimer,_SystemTimer->CurrentType());
				}*/
				Cycles -= m_CountPerOp;
				*_NextTimer -= m_CountPerOp;
				m_R4300i_Opcode[ Opcode.op ]();
				
				switch (R4300iOp::m_NextInstruction)
				{
				case NORMAL: 
					PROGRAM_COUNTER += 4; 
					break;
				case DELAY_SLOT:
					R4300iOp::m_NextInstruction = JUMP;
					PROGRAM_COUNTER += 4; 
					break;
				case JUMP:
					{
						BOOL CheckTimer = (JumpToLocation < PROGRAM_COUNTER || TestTimer); 
						PROGRAM_COUNTER  = JumpToLocation;
						R4300iOp::m_NextInstruction = NORMAL;
						if (CheckTimer)
						{
							TestTimer = FALSE;
							if (*_NextTimer < 0) 
							{ 
								_SystemTimer->TimerDone();
							}
							if (DoSomething)
							{
								_SystemEvents->ExecuteEvents();
							}
						}
					}
				}
			} else { 
				_Reg->DoTLBMiss(R4300iOp::m_NextInstruction == JUMP,PROGRAM_COUNTER);
				R4300iOp::m_NextInstruction = NORMAL;
			}
		}
	} __except( _MMU->MemoryFilter( GetExceptionCode(), GetExceptionInformation()) ) {
		DisplayError(GS(MSG_UNKNOWN_MEM_ACTION));
		ExitThread(0);
	}
}
