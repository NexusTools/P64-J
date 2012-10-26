#include <Windows.h>
#include <MMSystem.h>
#include "..\..\N64 System.h"
#include "..\C Core\c core.h"
#include "..\C Core\Recompiler Ops.h"
#include "..\C Core\X86.h"
#include "..\C Core\CPU.h"

#undef PROGRAM_COUNTER
#undef RdramSize
#undef LookUpMode
#undef LinkBlocks
#undef CountPerOp

CRecompiler::CRecompiler(CMipsMemory * MMU, CProfiling & Profile, bool & EndEmulation, bool SyncSystem) :
	_MMU(MMU),
	_Reg(MMU->SystemRegisters()),
	m_Profile(Profile),
	PROGRAM_COUNTER(_Reg->PROGRAM_COUNTER),
	m_EndEmulation(EndEmulation),
	m_SyncSystem(SyncSystem),
	m_Functions(),
	m_FunctionsDelaySlot()
{

}

CRecompiler::~CRecompiler()
{
}

void CRecompiler::Run()
{
	*g_MemoryStack = (DWORD)(N64MEM+(GPR[29].W[0] & 0x1FFFFFFF));
	CoInitialize(NULL);
	if (g_LogX86Code)
	{
		Start_x86_Log();
	}

	if (!g_MMU->AllocateRecompilerMemory(LookUpMode() != FuncFind_VirtualLookup && LookUpMode() != FuncFind_ChangeMemory)) 
	{ 
		return; 
	}
	JumpTable      = g_MMU->GetJumpTable();
	RecompCode     = g_MMU->GetRecompCode();

	ResetRecompCode();
	m_EndEmulation = false;

	__try {
		if (LookUpMode() == FuncFind_VirtualLookup)
		{
			if (bSMM_ValidFunc())
			{
				RecompilerMain_VirtualTable_validate();
			} else {
				RecompilerMain_VirtualTable();
			}
		}
		else if (LookUpMode() == FuncFind_ChangeMemory) 
		{
			RecompilerMain_ChangeMemory();
		} 
		else 
		{
			RecompilerMain_Lookup();
		}
	}
	__except( _MMU->SystemMemoryFilter( GetExceptionCode(), GetExceptionInformation()) ) 
	{
		Notify().DisplayError(MSG_UNKNOWN_MEM_ACTION);
	}
}

void CRecompiler::RecompilerMain_VirtualTable ( void )
{
	CFunctionMap::PFUNCTION_INFO_TABLE * m_FunctionTable = m_Functions.GetFunctionTable();

	while(!m_EndEmulation) 
	{
		if (NextInstruction != DELAY_SLOT) 
		{
			CFunctionMap::PFUNCTION_INFO_TABLE table = m_FunctionTable[PROGRAM_COUNTER >> 0xC];
			if (table)
			{
				FUNCTION_INFO * info = table[(PROGRAM_COUNTER & 0xFFF) >> 2];
				if (info != NULL)
				{
					const BYTE * Block = info->FunctionAddr();
					_asm {
						pushad
						call Block
						popad
					}
					continue;
				}
			}
			if (!_MMU->ValidVaddr(PROGRAM_COUNTER)) 
			{
				DoTLBMiss(NextInstruction == DELAY_SLOT,PROGRAM_COUNTER);
				NextInstruction = NORMAL;
				if (!_MMU->ValidVaddr(PROGRAM_COUNTER)) 
				{
					DisplayError("Failed to tranlate PC to a PAddr: %X\n\nEmulation stopped",PROGRAM_COUNTER);
					return;
				}
			}
			FUNCTION_INFO * info = CompilerCode();

			if (info == NULL || EndEmulation())
			{
				break;
			}
		} else {
			FUNCTION_INFO * Info = m_FunctionsDelaySlot.FindFunction(PROGRAM_COUNTER);
			//Find Block on hash table
			if (Info == NULL) 
			{
				if (!_MMU->ValidVaddr(PROGRAM_COUNTER)) 
				{
					DoTLBMiss(NextInstruction == DELAY_SLOT,PROGRAM_COUNTER);
					NextInstruction = NORMAL;
					if (!_MMU->ValidVaddr(PROGRAM_COUNTER)) 
					{
						DisplayError("Failed to tranlate PC to a PAddr: %X\n\nEmulation stopped",PROGRAM_COUNTER);
						return;
					}
					continue;
				}

				//Find Block on hash table
				Info = CompileDelaySlot(PROGRAM_COUNTER);

				if (Info == NULL || EndEmulation())
				{
					break;
				}
			} 
			const BYTE * Block = Info->FunctionAddr();
			_asm {
				pushad
				call Block
				popad
			}
			continue;
		}
	}
}

void CRecompiler::RecompilerMain_VirtualTable_validate ( void )
{
	CFunctionMap::PFUNCTION_INFO_TABLE * m_FunctionTable = m_Functions.GetFunctionTable();

	while(!m_EndEmulation) 
	{
		if (NextInstruction == DELAY_SLOT) 
		{
			FUNCTION_INFO * Info = m_FunctionsDelaySlot.FindFunction(PROGRAM_COUNTER);
			//Find Block on hash table
			if (Info == NULL) 
			{
				if (!_MMU->ValidVaddr(PROGRAM_COUNTER)) 
				{
					DoTLBMiss(NextInstruction == DELAY_SLOT,PROGRAM_COUNTER);
					NextInstruction = NORMAL;
					if (!_MMU->ValidVaddr(PROGRAM_COUNTER)) 
					{
						DisplayError("Failed to tranlate PC to a PAddr: %X\n\nEmulation stopped",PROGRAM_COUNTER);
						return;
					}
					continue;
				}

				//Find Block on hash table
				Info = CompileDelaySlot(PROGRAM_COUNTER);

				if (Info == NULL || EndEmulation())
				{
					break;
				}
			} 
			const BYTE * Block = Info->FunctionAddr();
			if ((*Info->MemLocation[0] != Info->MemContents[0]) ||
				(*Info->MemLocation[1] != Info->MemContents[1])) 
			{
				ClearRecompCode_Virt((Info->VStartPC() - 0x1000) & ~0xFFF,0x2000,Remove_ValidateFunc);
				NextInstruction = DELAY_SLOT;
				Info = NULL;
				continue;
			}
			_asm {
				pushad
				call Block
				popad
			}
			continue;
		}
		CFunctionMap::PFUNCTION_INFO_TABLE table = m_FunctionTable[PROGRAM_COUNTER >> 0xC];
		if (table)
		{
			FUNCTION_INFO * info = table[(PROGRAM_COUNTER & 0xFFF) >> 2];
			if (info != NULL)
			{
				if ((*info->MemLocation[0] != info->MemContents[0]) ||
					(*info->MemLocation[1] != info->MemContents[1])) 
				{
					ClearRecompCode_Virt((info->VStartPC() - 0x1000) & ~0xFFF,0x3000,Remove_ValidateFunc);
					info = NULL;
					continue;
				}
				const BYTE * Block = info->FunctionAddr();
				_asm {
					pushad
					call Block
					popad
				}
				continue;
			}
		}
		if (!_MMU->ValidVaddr(PROGRAM_COUNTER)) 
		{
			DoTLBMiss(NextInstruction == DELAY_SLOT,PROGRAM_COUNTER);
			NextInstruction = NORMAL;
			if (!_MMU->ValidVaddr(PROGRAM_COUNTER)) 
			{
				DisplayError("Failed to tranlate PC to a PAddr: %X\n\nEmulation stopped",PROGRAM_COUNTER);
				return;
			}
		}
		FUNCTION_INFO * info = CompilerCode();

		if (info == NULL || EndEmulation())
		{
			break;
		}

	}

/*
	while(!m_EndEmulation) 
	{
		if (!_MMU->ValidVaddr(PROGRAM_COUNTER)) 
		{
			DoTLBMiss(NextInstruction == DELAY_SLOT,PROGRAM_COUNTER);
			NextInstruction = NORMAL;
			if (!_MMU->ValidVaddr(PROGRAM_COUNTER)) 
			{
				DisplayError("Failed to tranlate PC to a PAddr: %X\n\nEmulation stopped",PROGRAM_COUNTER);
				return;
			}
		}
		if (NextInstruction == DELAY_SLOT) 
		{
			FUNCTION_INFO * Info = m_FunctionsDelaySlot.FindFunction(PROGRAM_COUNTER);

			//Find Block on hash table
			if (Info == NULL) 
			{
				Info = CompileDelaySlot(PROGRAM_COUNTER);

				if (Info == NULL || EndEmulation())
				{
					break;
				}
			} 
			if (bSMM_ValidFunc())
			{
				if ((*Info->MemLocation[0] != Info->MemContents[0]) ||
					(*Info->MemLocation[1] != Info->MemContents[1])) 
				{
					ClearRecompCode_Virt((Info->StartPC() - 0x1000) & ~0xFFF,0x2000,Remove_ValidateFunc);
					NextInstruction = DELAY_SLOT;
					Info = NULL;
					continue;
				}
			}
			const BYTE * Block = Info->FunctionAddr();
			_asm {
				pushad
				call Block
				popad
			}
			continue;
		}
		
		FUNCTION_INFO * Info = m_Functions.FindFunction(PROGRAM_COUNTER);

		//Find Block on hash table
		if (Info == NULL) 
		{
			Info = CompilerCode();

			if (Info == NULL || EndEmulation())
			{
				break;
			}
		} 
		if (bSMM_ValidFunc())
		{
			if ((*Info->MemLocation[0] != Info->MemContents[0]) ||
				(*Info->MemLocation[1] != Info->MemContents[1])) 
			{
				ClearRecompCode_Virt((Info->StartPC() - 0x1000) & ~0xFFF,0x3000,Remove_ValidateFunc);
				Info = NULL;
				continue;
			}
		}
		const BYTE * Block = Info->FunctionAddr();
		_asm {
			pushad
			call Block
			popad
		}
	}
	*/
}

void CRecompiler::RecompilerMain_Lookup( void )
{
	DWORD Addr;
	FUNCTION_INFO * Info;
	//const BYTE * Block;

	while(!m_EndEmulation) 
	{
		if (g_UseTlb)
		{
			if (!g_TLB->TranslateVaddr(PROGRAM_COUNTER, Addr))
			{
				DoTLBMiss(NextInstruction == DELAY_SLOT,PROGRAM_COUNTER);
				NextInstruction = NORMAL;
				if (!TranslateVaddr(PROGRAM_COUNTER, &Addr)) {
					DisplayError("Failed to tranlate PC to a PAddr: %X\n\nEmulation stopped",PROGRAM_COUNTER);
					return;
				}
			}
		} else {
			Addr = PROGRAM_COUNTER & 0x1FFFFFFF;
		}

		if (NextInstruction == DELAY_SLOT) {
			FUNCTION_INFO * Info = m_FunctionsDelaySlot.FindFunction(PROGRAM_COUNTER);

			//Find Block on hash table
			if (Info == NULL) 
			{
				Info = CompileDelaySlot(PROGRAM_COUNTER);

				if (Info == NULL || EndEmulation())
				{
					break;
				}
			} 
			if (bSMM_ValidFunc())
			{
				if ((*Info->MemLocation[0] != Info->MemContents[0]) ||
					(*Info->MemLocation[1] != Info->MemContents[1])) 
				{
					ClearRecompCode_Virt((Info->VStartPC() - 0x1000) & ~0xFFF,0x2000,Remove_ValidateFunc);
					NextInstruction = DELAY_SLOT;
					Info = NULL;
					continue;
				}
			}
			const BYTE * Block = Info->FunctionAddr();
			_asm {
				pushad
				call Block
				popad
			}
			continue;
		}

		__try {
			if (Addr > 0x10000000)
			{
				if (bRomInMemory())
				{
					if (Addr > 0x20000000)
					{
						WriteTraceF(TraceDebug,"Executing from non mapped space .1 PC: %X Addr: %X",PROGRAM_COUNTER, Addr);
						DisplayError(GS(MSG_NONMAPPED_SPACE));
						break;
					}
					Info = (FUNCTION_INFO *)*(JumpTable + (Addr >> 2));
				} else {
					if (PROGRAM_COUNTER >= 0xB0000000 && PROGRAM_COUNTER < (RomFileSize | 0xB0000000)) {
						while (PROGRAM_COUNTER >= 0xB0000000 && PROGRAM_COUNTER < (RomFileSize | 0xB0000000)) {
							ExecuteInterpreterOpCode();
						}
						continue;
					} else {
						WriteTraceF(TraceDebug,"Executing from non mapped space .1 PC: %X Addr: %X",PROGRAM_COUNTER, Addr);
						DisplayError(GS(MSG_NONMAPPED_SPACE));
						break;
					}
				}
			} else {
				Info = (FUNCTION_INFO *)*(JumpTable + (Addr >> 2));
			}
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			if (PROGRAM_COUNTER >= 0xB0000000 && PROGRAM_COUNTER < (RomFileSize | 0xB0000000)) {
				while (PROGRAM_COUNTER >= 0xB0000000 && PROGRAM_COUNTER < (RomFileSize | 0xB0000000)) {
					ExecuteInterpreterOpCode();
				}
				continue;
			} else {
					WriteTraceF(TraceDebug,"Executing from non mapped space .2 PC: %X Addr: %X",PROGRAM_COUNTER, Addr);
				DisplayError(GS(MSG_NONMAPPED_SPACE));
				return;
			}
		}
		
		if (Info == NULL) 
		{
			Info = CompilerCode();

			if (Info == NULL || EndEmulation())
			{
				break;
			}
			*(JumpTable + (Addr >> 2)) = (void *)Info;

//			if (SelfModCheck == ModCode_ProtectedMemory) {
//				VirtualProtect(N64MEM + Addr, 4, PAGE_READONLY, &OldProtect);
//			}
		}
		if (bSMM_ValidFunc())
		{
			if ((*Info->MemLocation[0] != Info->MemContents[0]) ||
				(*Info->MemLocation[1] != Info->MemContents[1])) 
			{
				ClearRecompCode_Virt((Info->VStartPC() - 0x1000) & ~0xFFF,0x3000,Remove_ValidateFunc);
				Info = NULL;
				continue;
			}
		}
#ifdef tofix
		if (Profiling && IndvidualBlock) {
			static DWORD ProfAddress = 0;

			if ((PROGRAM_COUNTER & ~0xFFF) != ProfAddress) {
				char Label[100];

				ProfAddress = PROGRAM_COUNTER & ~0xFFF;
				sprintf(Label,"PC: %X to %X",ProfAddress,ProfAddress+ 0xFFC);
//						StartTimer(Label);				
			}
			/*if (PROGRAM_COUNTER >= 0x800DD000 && PROGRAM_COUNTER <= 0x800DDFFC) {
				char Label[100];
				sprintf(Label,"PC: %X   Block: %X",PROGRAM_COUNTER,Block);
				StartTimer(Label);				
			}*/
//				} else 	if ((Profiling || ShowCPUPer) && ProfilingLabel[0] == 0) { 
//					StartTimer("r4300i Running"); 
		}
#endif
		const BYTE * Block = Info->FunctionAddr();
		_asm {
			pushad
			call Block
			popad
		}
	}
}

void CRecompiler::ResetRecompCode()
{
	RecompPos = RecompCode;

	m_Functions.Reset();
	m_FunctionsDelaySlot.Reset();
	if (JumpTable)
	{
		memset(JumpTable,0,g_MMU->RdramSize());
		memset(JumpTable + (0x04000000 >> 2),0,0x1000);
		memset(JumpTable + (0x04001000 >> 2),0,0x1000);
		if (bRomInMemory())
		{
			memset(JumpTable + (0x10000000 >> 2),0,RomFileSize);
		}
	}
#ifdef to_clean
	DWORD count, OldProtect;
	if (SelfModCheck == ModCode_ChangeMemory) {
		DWORD count, PAddr, Value;

		for (count = 0; count < TargetIndex; count++) {
			PAddr = OrigMem[(WORD)(count)].PAddr;
			Value = *(DWORD *)(N64MEM + PAddr);
			if ( ((Value >> 16) == 0x7C7C) && ((Value & 0xFFFF) == count)) {
				*(DWORD *)(N64MEM + PAddr) = OrigMem[(WORD)(count)].OriginalValue;
			} 			
		}
	}
	TargetIndex = 0;
	
	//Jump Table
	for (count = 0; count < (RdramSize >> 12); count ++ ) {
		if (N64_Blocks.NoOfRDRamBlocks[count] > 0) {
			N64_Blocks.NoOfRDRamBlocks[count] = 0;		
			memset(JumpTable + (count << 10),0,0x1000);
			*(DelaySlotTable + count) = NULL;

			if (VirtualProtect((N64MEM + (count << 12)), 4, PAGE_READWRITE, &OldProtect) == 0) {
				DisplayError("Failed to unprotect %X\n1", (count << 12));
			}
		}			
	}
	
	if (N64_Blocks.NoOfDMEMBlocks > 0) {
		N64_Blocks.NoOfDMEMBlocks = 0;
		memset(JumpTable + (0x04000000 >> 2),0,0x1000);
		*(DelaySlotTable + (0x04000000 >> 12)) = NULL;
		if (VirtualProtect((N64MEM + 0x04000000), 4, PAGE_READWRITE, &OldProtect) == 0) {
			DisplayError("Failed to unprotect %X\n0", 0x04000000);
		}
	}
	if (N64_Blocks.NoOfIMEMBlocks > 0) {
		N64_Blocks.NoOfIMEMBlocks = 0;
		memset(JumpTable + (0x04001000 >> 2),0,0x1000);
		*(DelaySlotTable + (0x04001000 >> 12)) = NULL;
		if (VirtualProtect((N64MEM + 0x04001000), 4, PAGE_READWRITE, &OldProtect) == 0) {
			DisplayError("Failed to unprotect %X\n4", 0x04001000);
		}
	}	
//	if (N64_Blocks.NoOfPifRomBlocks > 0) {
//		N64_Blocks.NoOfPifRomBlocks = 0;
//		memset(JumpTable + (0x1FC00000 >> 2),0,0x1000);
//	}
#endif
}

FUNCTION_INFO * CRecompiler::CompileDelaySlot(DWORD PC) 
{
	WriteTraceF(TraceRecompiler,"Compile Delay Slot: %X",PC);
	if ((PC & 0xFFC) != 0) {
		DisplayError("Why are you compiling the Delay Slot at %X",PC);
		return NULL;
	}

	if (!r4300i_LW_VAddr(PC, &g_Opcode.Hex)) {
		DisplayError("TLB Miss in delay slot\nEmulation will know stop");
		return NULL;
	} 

	FUNCTION_INFO * info = m_FunctionsDelaySlot.AddFunctionInfo(PC, _MMU->TranslateVaddr(PC));
	
	CBlockInfo BlockInfo(PROGRAM_COUNTER, RecompPos);
	CBlockSection * Section = &BlockInfo.ParentSection;

	BYTE * Block = RecompPos;
	DWORD StartAddress;
	if (!TranslateVaddr(PC, &StartAddress))
	{
		return NULL;
	}
	if (StartAddress < RdramSize()) {
		CPU_Message("====== RDRAM: Delay Slot ======");
	} else if (StartAddress >= 0x04000000 && StartAddress <= 0x04000FFC) {
		CPU_Message("====== DMEM: Delay Slot ======");
	} else if (StartAddress >= 0x04001000 && StartAddress <= 0x04001FFC) {
		CPU_Message("====== IMEM: Delay Slot ======");
	} else if (StartAddress >= 0x1FC00000 && StartAddress <= 0x1FC00800) {
		CPU_Message("====== PIF ROM: Delay Slot ======");
	} else {
		CPU_Message("====== Unknown: Delay Slot ======");
	}
	CPU_Message("x86 code at: %X",Block);
	CPU_Message("Delay Slot location: %X",PROGRAM_COUNTER );
	CPU_Message("====== recompiled code ======");

	Section->AddParent(NULL);
	Section->BlockCycleCount() += CountPerOp();
	Section->BlockRandomModifier() += 1;
			
	switch (Opcode.op) {
	case R4300i_SPECIAL:
		switch (Opcode.funct) {
		case R4300i_SPECIAL_SLL: Compile_R4300i_SPECIAL_SLL(Section); break;
		case R4300i_SPECIAL_SRL: Compile_R4300i_SPECIAL_SRL(Section); break;
		case R4300i_SPECIAL_SRA: Compile_R4300i_SPECIAL_SRA(Section); break;
		case R4300i_SPECIAL_SLLV: Compile_R4300i_SPECIAL_SLLV(Section); break;
		case R4300i_SPECIAL_SRLV: Compile_R4300i_SPECIAL_SRLV(Section); break;
		case R4300i_SPECIAL_SRAV: Compile_R4300i_SPECIAL_SRAV(Section); break;
		case R4300i_SPECIAL_MFLO: Compile_R4300i_SPECIAL_MFLO(Section); break;
		case R4300i_SPECIAL_MTLO: Compile_R4300i_SPECIAL_MTLO(Section); break;
		case R4300i_SPECIAL_MFHI: Compile_R4300i_SPECIAL_MFHI(Section); break;
		case R4300i_SPECIAL_MTHI: Compile_R4300i_SPECIAL_MTHI(Section); break;
		case R4300i_SPECIAL_MULT: Compile_R4300i_SPECIAL_MULT(Section); break;
		case R4300i_SPECIAL_DIV: Compile_R4300i_SPECIAL_DIV(Section); break;
		case R4300i_SPECIAL_DIVU: Compile_R4300i_SPECIAL_DIVU(Section); break;
		case R4300i_SPECIAL_MULTU: Compile_R4300i_SPECIAL_MULTU(Section); break;
		case R4300i_SPECIAL_DMULTU: Compile_R4300i_SPECIAL_DMULTU(Section); break;
		case R4300i_SPECIAL_DDIVU: Compile_R4300i_SPECIAL_DDIVU(Section); break;
		case R4300i_SPECIAL_ADD: Compile_R4300i_SPECIAL_ADD(Section); break;
		case R4300i_SPECIAL_ADDU: Compile_R4300i_SPECIAL_ADDU(Section); break;
		case R4300i_SPECIAL_SUB: Compile_R4300i_SPECIAL_SUB(Section); break;
		case R4300i_SPECIAL_SUBU: Compile_R4300i_SPECIAL_SUBU(Section); break;
		case R4300i_SPECIAL_AND: Compile_R4300i_SPECIAL_AND(Section); break;
		case R4300i_SPECIAL_OR: Compile_R4300i_SPECIAL_OR(Section); break;
		case R4300i_SPECIAL_XOR: Compile_R4300i_SPECIAL_XOR(Section); break;
		case R4300i_SPECIAL_SLT: Compile_R4300i_SPECIAL_SLT(Section); break;
		case R4300i_SPECIAL_SLTU: Compile_R4300i_SPECIAL_SLTU(Section); break;
		case R4300i_SPECIAL_DADD: Compile_R4300i_SPECIAL_DADD(Section); break;
		case R4300i_SPECIAL_DADDU: Compile_R4300i_SPECIAL_DADDU(Section); break;
		case R4300i_SPECIAL_DSLL32: Compile_R4300i_SPECIAL_DSLL32(Section); break;
		case R4300i_SPECIAL_DSRA32: Compile_R4300i_SPECIAL_DSRA32(Section); break;
		default:
			Compile_R4300i_UnknownOpcode(Section); break;
		}
		break;
	case R4300i_ADDI: Compile_R4300i_ADDI(Section); break;
	case R4300i_ADDIU: Compile_R4300i_ADDIU(Section); break;
	case R4300i_SLTI: Compile_R4300i_SLTI(Section); break;
	case R4300i_SLTIU: Compile_R4300i_SLTIU(Section); break;
	case R4300i_ANDI: Compile_R4300i_ANDI(Section); break;
	case R4300i_ORI: Compile_R4300i_ORI(Section); break;
	case R4300i_XORI: Compile_R4300i_XORI(Section); break;
	case R4300i_LUI: Compile_R4300i_LUI(Section); break;
	case R4300i_CP1:
		switch (Opcode.rs) {
		case R4300i_COP1_CF: Compile_R4300i_COP1_CF(Section); break;
		case R4300i_COP1_MT: Compile_R4300i_COP1_MT(Section); break;
		case R4300i_COP1_CT: Compile_R4300i_COP1_CT(Section); break;
		case R4300i_COP1_MF: Compile_R4300i_COP1_MF(Section); break;
		case R4300i_COP1_S: 
			switch (Opcode.funct) {
			case R4300i_COP1_FUNCT_ADD: Compile_R4300i_COP1_S_ADD(Section); break;
			case R4300i_COP1_FUNCT_SUB: Compile_R4300i_COP1_S_SUB(Section); break;
			case R4300i_COP1_FUNCT_MUL: Compile_R4300i_COP1_S_MUL(Section); break;
			case R4300i_COP1_FUNCT_DIV: Compile_R4300i_COP1_S_DIV(Section); break;
			case R4300i_COP1_FUNCT_ABS: Compile_R4300i_COP1_S_ABS(Section); break;
			case R4300i_COP1_FUNCT_NEG: Compile_R4300i_COP1_S_NEG(Section); break;
			case R4300i_COP1_FUNCT_SQRT: Compile_R4300i_COP1_S_SQRT(Section); break;
			case R4300i_COP1_FUNCT_MOV: Compile_R4300i_COP1_S_MOV(Section); break;
			case R4300i_COP1_FUNCT_CVT_D: Compile_R4300i_COP1_S_CVT_D(Section); break;
			case R4300i_COP1_FUNCT_ROUND_W: Compile_R4300i_COP1_S_ROUND_W(Section); break;
			case R4300i_COP1_FUNCT_TRUNC_W: Compile_R4300i_COP1_S_TRUNC_W(Section); break;
			case R4300i_COP1_FUNCT_FLOOR_W: Compile_R4300i_COP1_S_FLOOR_W(Section); break;
			case R4300i_COP1_FUNCT_C_F:   case R4300i_COP1_FUNCT_C_UN:
			case R4300i_COP1_FUNCT_C_EQ:  case R4300i_COP1_FUNCT_C_UEQ:
			case R4300i_COP1_FUNCT_C_OLT: case R4300i_COP1_FUNCT_C_ULT:
			case R4300i_COP1_FUNCT_C_OLE: case R4300i_COP1_FUNCT_C_ULE:
			case R4300i_COP1_FUNCT_C_SF:  case R4300i_COP1_FUNCT_C_NGLE:
			case R4300i_COP1_FUNCT_C_SEQ: case R4300i_COP1_FUNCT_C_NGL:
			case R4300i_COP1_FUNCT_C_LT:  case R4300i_COP1_FUNCT_C_NGE:
			case R4300i_COP1_FUNCT_C_LE:  case R4300i_COP1_FUNCT_C_NGT:
				Compile_R4300i_COP1_S_CMP(Section); break;
			default:
				Compile_R4300i_UnknownOpcode(Section); break;
			}
			break;
		case R4300i_COP1_D: 
			switch (Opcode.funct) {
			case R4300i_COP1_FUNCT_ADD: Compile_R4300i_COP1_D_ADD(Section); break;
			case R4300i_COP1_FUNCT_SUB: Compile_R4300i_COP1_D_SUB(Section); break;
			case R4300i_COP1_FUNCT_MUL: Compile_R4300i_COP1_D_MUL(Section); break;
			case R4300i_COP1_FUNCT_DIV: Compile_R4300i_COP1_D_DIV(Section); break;
			case R4300i_COP1_FUNCT_ABS: Compile_R4300i_COP1_D_ABS(Section); break;
			case R4300i_COP1_FUNCT_NEG: Compile_R4300i_COP1_D_NEG(Section); break;
			case R4300i_COP1_FUNCT_SQRT: Compile_R4300i_COP1_D_SQRT(Section); break;
			case R4300i_COP1_FUNCT_MOV: Compile_R4300i_COP1_D_MOV(Section); break;
			case R4300i_COP1_FUNCT_TRUNC_W: Compile_R4300i_COP1_D_TRUNC_W(Section); break;
			case R4300i_COP1_FUNCT_CVT_S: Compile_R4300i_COP1_D_CVT_S(Section); break;
			case R4300i_COP1_FUNCT_CVT_W: Compile_R4300i_COP1_D_CVT_W(Section); break;
			case R4300i_COP1_FUNCT_C_F:   case R4300i_COP1_FUNCT_C_UN:
			case R4300i_COP1_FUNCT_C_EQ:  case R4300i_COP1_FUNCT_C_UEQ:
			case R4300i_COP1_FUNCT_C_OLT: case R4300i_COP1_FUNCT_C_ULT:
			case R4300i_COP1_FUNCT_C_OLE: case R4300i_COP1_FUNCT_C_ULE:
			case R4300i_COP1_FUNCT_C_SF:  case R4300i_COP1_FUNCT_C_NGLE:
			case R4300i_COP1_FUNCT_C_SEQ: case R4300i_COP1_FUNCT_C_NGL:
			case R4300i_COP1_FUNCT_C_LT:  case R4300i_COP1_FUNCT_C_NGE:
			case R4300i_COP1_FUNCT_C_LE:  case R4300i_COP1_FUNCT_C_NGT:
				Compile_R4300i_COP1_D_CMP(Section); break;
			default:
				Compile_R4300i_UnknownOpcode(Section); break;
			}
			break;
		case R4300i_COP1_W: 
			switch (Opcode.funct) {
			case R4300i_COP1_FUNCT_CVT_S: Compile_R4300i_COP1_W_CVT_S(Section); break;
			case R4300i_COP1_FUNCT_CVT_D: Compile_R4300i_COP1_W_CVT_D(Section); break;
			default:
				Compile_R4300i_UnknownOpcode(Section); break;
			}
			break;
		default:
			Compile_R4300i_UnknownOpcode(Section); break;
		}
		break;
	case R4300i_LB: Compile_R4300i_LB(Section); break;
	case R4300i_LH: Compile_R4300i_LH(Section); break;
	case R4300i_LW: Compile_R4300i_LW(Section); break;
	case R4300i_LBU: Compile_R4300i_LBU(Section); break;
	case R4300i_LHU: Compile_R4300i_LHU(Section); break;
	case R4300i_SB: Compile_R4300i_SB(Section); break;
	case R4300i_SH: Compile_R4300i_SH(Section); break;
	case R4300i_SW: Compile_R4300i_SW(Section); break;
	case R4300i_SWR: Compile_R4300i_SWR(Section); break;
	case R4300i_CACHE: Compile_R4300i_CACHE(Section); break;
	case R4300i_LWC1: Compile_R4300i_LWC1(Section); break;
	case R4300i_LDC1: Compile_R4300i_LDC1(Section); break;
	case R4300i_LD: Compile_R4300i_LD(Section); break;
	case R4300i_SWC1: Compile_R4300i_SWC1(Section); break;
	case R4300i_SDC1: Compile_R4300i_SDC1(Section); break;
	case R4300i_SD: Compile_R4300i_SD(Section); break;
	default:
		Compile_R4300i_UnknownOpcode(Section); break;
	}

	Section->ResetX86Protection();
	
	WriteBackRegisters(Section);
	UpdateCounters(&Section->BlockCycleCount(),&Section->BlockRandomModifier(), FALSE);
	int x86Reg = Map_TempReg(Section,x86_Any,-1,FALSE);
	MoveVariableToX86reg(&JumpToLocation,"JumpToLocation",x86Reg);
	MoveX86regToVariable(x86Reg,&PROGRAM_COUNTER,"PROGRAM_COUNTER");
	MoveConstToVariable(NORMAL,&NextInstruction,"NextInstruction");
	if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
	Ret();
	CompileExitCode(BlockInfo);
	CPU_Message("====== End of recompiled code ======");
	
	info->SetVEndPC(BlockInfo.EndVAddr);
	info->SetFunctionAddr(BlockInfo.CompiledLocation);
	_MMU->VAddrToRealAddr(info->VStartPC(),*(reinterpret_cast<void **>(&info->MemLocation[0])));
	info->MemLocation[1] = info->MemLocation[0] + 1;
	info->MemContents[0] = *info->MemLocation[0];
	info->MemContents[1] = *info->MemLocation[1];
	NextInstruction = NORMAL;
	return info;
}

bool CRecompiler::AnalyseBlock ( CBlockInfo & BlockInfo) 
{
	if (bLinkBlocks())
	{ 	
		CBlockSection * Section = &BlockInfo.ParentSection;
		if (!CreateSectionLinkage (Section)) { return false; }
		DetermineLoop(Section,CBlockSection::GetNewTestValue(),CBlockSection::GetNewTestValue(), Section->SectionID);
		while (FixConstants(Section,CBlockSection::GetNewTestValue())) {}
	}
	return true;
}

bool CRecompiler::FixConstants (CBlockSection * Section, DWORD Test)
{
	if (Section == NULL) { return false; }
	if (Section->Test == Test) { return false; }
	Section->Test = Test;

	InheritConstants(Section);

	bool Changed = false;
/*	BLOCK_SECTION * Parent;
	int count, NoOfParents;
	REG_INFO Original[2];
*/
	CRegInfo Original[2] = { Section->Cont.RegSet, Section->Jump.RegSet };

	if (!Section->ParentSection.empty()) {
		for (SECTION_LIST::iterator iter = Section->ParentSection.begin(); iter != Section->ParentSection.end(); iter++)
		{
			CBlockSection * Parent = *iter;
			if (Parent->ContinueSection == Section) {
				for (int count = 0; count < 32; count++) {
					if (Section->RegStart.MipsRegState(count) != Parent->Cont.RegSet.MipsRegState(count)) {
						Section->RegStart.MipsRegState(count) = CRegInfo::STATE_UNKNOWN;							
						//*Changed = true;
					}
					Section->RegStart.MipsRegState(count) = CRegInfo::STATE_UNKNOWN;							
				}
			}
			if (Parent->JumpSection == Section) {
				for (int count = 0; count < 32; count++) {
					if (Section->RegStart.MipsRegState(count) != Parent->Jump.RegSet.MipsRegState(count)) {
						Section->RegStart.MipsRegState(count) = CRegInfo::STATE_UNKNOWN;
						//*Changed = true;
					}
				}
			}
			Section->RegWorking = Section->RegStart;
		}
	}

	FillSectionInfo(Section, NORMAL);
	if (Original[0] != Section->Cont.RegSet) 
	{
		Changed = true; 
	}
	if (Original[1] != Section->Jump.RegSet) 
	{
		Changed = true;
	}
	
	if (Section->JumpSection && FixConstants(Section->JumpSection,Test)) { Changed = true; }
	if (Section->ContinueSection && FixConstants(Section->ContinueSection,Test)) { Changed = true; }
	
	return Changed;
}

void CRecompiler::InheritConstants(CBlockSection * Section)
{
	if (Section->ParentSection.empty())
	{
		Section->RegStart.Initilize();
		Section->RegWorking = Section->RegStart;
		return;
	}

	CBlockSection * Parent = *(Section->ParentSection.begin());
	CRegInfo * RegSet = (Section == Parent->ContinueSection?&Parent->Cont.RegSet:&Parent->Jump.RegSet);
	Section->RegStart = *RegSet;
	Section->RegWorking = *RegSet;		

	for (SECTION_LIST::iterator iter = Section->ParentSection.begin(); iter != Section->ParentSection.end(); iter++)
	{
		if (iter == Section->ParentSection.begin()) { continue; }
		Parent = *iter;
		RegSet = Section == Parent->ContinueSection?&Parent->Cont.RegSet:&Parent->Jump.RegSet;
			
		for (int count = 0; count < 32; count++) {
			if (Section->IsConst(count)) {
				if (Section->MipsRegState(count) != RegSet->MipsRegState(count)) {
					Section->MipsRegState(count) = CRegInfo::STATE_UNKNOWN;
				} else if (Section->Is32Bit(count) && Section->MipsRegLo(count) != RegSet->MipsRegLo(count)) {
					Section->MipsRegState(count) = CRegInfo::STATE_UNKNOWN;
				} else if (Section->Is64Bit(count) && Section->MipsReg(count) != RegSet->MipsReg(count)) {
					Section->MipsRegState(count) = CRegInfo::STATE_UNKNOWN;
				}
			}
		}
	}
	Section->RegStart = Section->RegWorking;
}

CBlockSection * CRecompiler::ExistingSection(CBlockSection * StartSection, DWORD Addr, DWORD Test) 
{
	if (StartSection == NULL) { return NULL; }
	if (StartSection->StartPC == Addr && StartSection->LinkAllowed) 
	{ 
		return StartSection; 
	}
	if (StartSection->Test == Test) { return NULL; }
	StartSection->Test = Test;

	CBlockSection * Section = ExistingSection(StartSection->JumpSection,Addr,Test);
	if (Section != NULL) { return Section; }
	Section = ExistingSection(StartSection->ContinueSection,Addr,Test);
	if (Section != NULL) { return Section; }
	return NULL;
}

bool CRecompiler::CreateSectionLinkage (CBlockSection * Section) {
	InheritConstants(Section);

	if (!FillSectionInfo(Section,NORMAL))
	{
		return false;
	}
	
	CBlockSection ** TargetSection[2];
	CJumpInfo * JumpInfo[2];
	if (Section->Jump.TargetPC < Section->Cont.TargetPC) {
		TargetSection[0] = (CBlockSection **)&Section->JumpSection;
		TargetSection[1] = (CBlockSection **)&Section->ContinueSection;
		JumpInfo[0] = &Section->Jump;
		JumpInfo[1] = &Section->Cont;	
	} else {
		TargetSection[0] = (CBlockSection **)&Section->ContinueSection;
		TargetSection[1] = (CBlockSection **)&Section->JumpSection;
		JumpInfo[0] = &Section->Cont;	
		JumpInfo[1] = &Section->Jump;
	}

	CBlockInfo * BlockInfo = Section->BlockInfo;

	for (int count = 0; count < 2; count ++) 
	{
		if (JumpInfo[count]->TargetPC == (DWORD)-1 || *TargetSection[count] != NULL) 
		{
			continue;
		}
		if (!JumpInfo[count]->DoneDelaySlot)
		{
			Section->Jump.RegSet = Section->RegWorking;

			//this is a special delay slot section
			BlockInfo->NoOfSections += 1;
			*TargetSection[count] = new CBlockSection(BlockInfo,Section->CompilePC + 4,BlockInfo->NoOfSections);
			(*TargetSection[count])->AddParent(Section);
			(*TargetSection[count])->LinkAllowed = false;
			InheritConstants((*TargetSection[count]));

			if (!FillSectionInfo((*TargetSection[count]),END_BLOCK))
			{
				return false;
			}
			(*TargetSection[count])->Jump.TargetPC = -1;
			(*TargetSection[count])->Cont.TargetPC = JumpInfo[count]->TargetPC;
			(*TargetSection[count])->Cont.FallThrough = true;
			(*TargetSection[count])->Cont.RegSet = (*TargetSection[count])->RegWorking;
			JumpInfo[count]->TargetPC = Section->CompilePC + 4;

			//Create the section that joins with that block
			(*TargetSection[count])->ContinueSection = ExistingSection(&BlockInfo->ParentSection,(*TargetSection[count])->Cont.TargetPC,CBlockSection::GetNewTestValue());
			if ((*TargetSection[count])->ContinueSection == NULL) {
				BlockInfo->NoOfSections += 1;
				(*TargetSection[count])->ContinueSection = new CBlockSection(BlockInfo,(*TargetSection[count])->Cont.TargetPC,BlockInfo->NoOfSections);
				(*TargetSection[count])->ContinueSection->AddParent((*TargetSection[count]));
				CreateSectionLinkage((*TargetSection[count])->ContinueSection);
			} else {
				(*TargetSection[count])->ContinueSection->AddParent((*TargetSection[count]));
			}
		} else { 	
			*TargetSection[count] = ExistingSection(&BlockInfo->ParentSection,JumpInfo[count]->TargetPC,CBlockSection::GetNewTestValue());
			if (*TargetSection[count] == NULL) {
				BlockInfo->NoOfSections += 1;
				*TargetSection[count] = new CBlockSection(BlockInfo,JumpInfo[count]->TargetPC,BlockInfo->NoOfSections);
				(*TargetSection[count])->AddParent(Section);
				CreateSectionLinkage(*TargetSection[count]);
			} else {
				(*TargetSection[count])->AddParent(Section);
			}
		}
	}
	return true;
}

bool CRecompiler::FillSectionInfo(CBlockSection * Section, STEP_TYPE StartStepType) 
{
	OPCODE Command;

	if (Section->CompiledLocation != NULL) { return true; }
	Section->CompilePC  = Section->StartPC;
	Section->RegWorking = Section->RegStart;
	NextInstruction = StartStepType;
	do {
		if (!r4300i_LW_VAddr(Section->CompilePC, &Command.Hex)) {
			DisplayError(GS(MSG_FAIL_LOAD_WORD));
			return false;
		}		
		switch (Command.op) {
		case R4300i_SPECIAL:
			switch (Command.funct) {
			case R4300i_SPECIAL_SLL: 
				if (Command.rd == 0) { break; }
				if (Section->InLoop && Command.rt == Command.rd) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_32;
					Section->MipsRegLo(Command.rd) = Section->MipsRegLo(Command.rt) << Command.sa;
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SRL: 
				if (Command.rd == 0) { break; }
				if (Section->InLoop && Command.rt == Command.rd) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_32;
					Section->MipsRegLo(Command.rd) = Section->MipsRegLo(Command.rt) >> Command.sa;
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SRA: 
				if (Command.rd == 0) { break; }
				if (Section->InLoop && Command.rt == Command.rd) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_32;
					Section->MipsRegLo(Command.rd) = Section->MipsRegLo_S(Command.rt) >> Command.sa;
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SLLV: 
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt) && Section->IsConst(Command.rs)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_32;
					Section->MipsRegLo(Command.rd) = Section->MipsRegLo(Command.rt) << (Section->MipsRegLo(Command.rs) & 0x1F);
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SRLV: 
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt) && Section->IsConst(Command.rs)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_32;
					Section->MipsRegLo(Command.rd) = Section->MipsRegLo(Command.rt) >> (Section->MipsRegLo(Command.rs) & 0x1F);
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SRAV: 
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt) && Section->IsConst(Command.rs)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_32;
					Section->MipsRegLo(Command.rd) = Section->MipsRegLo_S(Command.rt) >> (Section->MipsRegLo(Command.rs) & 0x1F);
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_JR:				
				if (Section->IsConst(Command.rs)) {
					Section->Jump.TargetPC = Section->MipsRegLo(Command.rs);
				} else {
					Section->Jump.TargetPC = (DWORD)-1;
				}
				NextInstruction = DELAY_SLOT;
				break;
			case R4300i_SPECIAL_JALR: 
				Section->MipsRegLo(Opcode.rd) = Section->CompilePC + 8;
				Section->MipsRegState(Opcode.rd) = CRegInfo::STATE_CONST_32;
				if (Section->IsConst(Command.rs)) {
					Section->Jump.TargetPC = Section->MipsRegLo(Command.rs);
				} else {
					Section->Jump.TargetPC = (DWORD)-1;
				}
				NextInstruction = DELAY_SLOT;
				break;
			case R4300i_SPECIAL_SYSCALL:
			case R4300i_SPECIAL_BREAK:
				NextInstruction = END_BLOCK;
				Section->CompilePC -= 4;
				break;
			case R4300i_SPECIAL_MFHI: Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN; break;
			case R4300i_SPECIAL_MTHI: break;
			case R4300i_SPECIAL_MFLO: Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN; break;
			case R4300i_SPECIAL_MTLO: break;
			case R4300i_SPECIAL_DSLLV: 
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt) && Section->IsConst(Command.rs)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_64;
					Section->MipsReg(Command.rd) = Section->Is64Bit(Command.rt)?Section->MipsReg(Command.rt):(QWORD)Section->MipsRegLo_S(Command.rt) << (Section->MipsRegLo(Command.rs) & 0x3F);
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSRLV: 
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt) && Section->IsConst(Command.rs)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_64;
					Section->MipsReg(Command.rd) = Section->Is64Bit(Command.rt)?Section->MipsReg(Command.rt):(QWORD)Section->MipsRegLo_S(Command.rt) >> (Section->MipsRegLo(Command.rs) & 0x3F);
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSRAV: 
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt) && Section->IsConst(Command.rs)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_64;
					Section->MipsReg(Command.rd) = Section->Is64Bit(Command.rt)?Section->MipsReg_S(Command.rt):(_int64)Section->MipsRegLo_S(Command.rt) >> (Section->MipsRegLo(Command.rs) & 0x3F);
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_MULT: break;
			case R4300i_SPECIAL_MULTU: break;
			case R4300i_SPECIAL_DIV: break;
			case R4300i_SPECIAL_DIVU: break;
			case R4300i_SPECIAL_DMULT: break;
			case R4300i_SPECIAL_DMULTU: break;
			case R4300i_SPECIAL_DDIV: break;
			case R4300i_SPECIAL_DDIVU: break;
			case R4300i_SPECIAL_ADD:
			case R4300i_SPECIAL_ADDU:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt) && Section->IsConst(Command.rs)) {
					Section->MipsRegLo(Command.rd) = Section->MipsRegLo(Command.rs) + Section->MipsRegLo(Command.rt);
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_32;
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SUB: 
			case R4300i_SPECIAL_SUBU: 
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt) && Section->IsConst(Command.rs)) {
					Section->MipsRegLo(Command.rd) = Section->MipsRegLo(Command.rs) - Section->MipsRegLo(Command.rt);
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_32;
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_AND: 
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt) && Section->IsConst(Command.rs)) {
					if (Section->Is64Bit(Command.rt) && Section->Is64Bit(Command.rs)) {
						Section->MipsReg(Command.rd) = Section->MipsReg(Command.rt) & Section->MipsReg(Command.rs);
						Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_64;					
					} else if (Section->Is64Bit(Command.rt) || Section->Is64Bit(Command.rs)) {
						if (Section->Is64Bit(Command.rt)) {
							Section->MipsReg(Command.rd) = Section->MipsReg(Command.rt) & Section->MipsRegLo(Command.rs);
						} else {
							Section->MipsReg(Command.rd) = Section->MipsRegLo(Command.rt) & Section->MipsReg(Command.rs);
						}						
						Section->MipsRegState(Command.rd) = CRegInfo::ConstantsType(Section->MipsReg(Command.rd));
					} else {
						Section->MipsRegLo(Command.rd) = Section->MipsRegLo(Command.rt) & Section->MipsRegLo(Command.rs);
						Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_32;
					}
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_OR: 
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt) && Section->IsConst(Command.rs)) {
					if (Section->Is64Bit(Command.rt) && Section->Is64Bit(Command.rs)) {
						Section->MipsReg(Command.rd) = Section->MipsReg(Command.rt) | Section->MipsReg(Command.rs);
						Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_64;
					} else if (Section->Is64Bit(Command.rt) || Section->Is64Bit(Command.rs)) {
						if (Section->Is64Bit(Command.rt)) {
							Section->MipsReg(Command.rd) = Section->MipsReg(Command.rt) | Section->MipsRegLo(Command.rs);
						} else {
							Section->MipsReg(Command.rd) = Section->MipsRegLo(Command.rt) | Section->MipsReg(Command.rs);
						}
						Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_64;
					} else {
						Section->MipsRegLo(Command.rd) = Section->MipsRegLo(Command.rt) | Section->MipsRegLo(Command.rs);
						Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_32;
					}
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_XOR: 
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt) && Section->IsConst(Command.rs)) {
					if (Section->Is64Bit(Command.rt) && Section->Is64Bit(Command.rs)) {
						Section->MipsReg(Command.rd) = Section->MipsReg(Command.rt) ^ Section->MipsReg(Command.rs);
						Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_64;
					} else if (Section->Is64Bit(Command.rt) || Section->Is64Bit(Command.rs)) {
						if (Section->Is64Bit(Command.rt)) {
							Section->MipsReg(Command.rd) = Section->MipsReg(Command.rt) ^ Section->MipsRegLo(Command.rs);
						} else {
							Section->MipsReg(Command.rd) = Section->MipsRegLo(Command.rt) ^ Section->MipsReg(Command.rs);
						}
						Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_64;
					} else {
						Section->MipsRegLo(Command.rd) = Section->MipsRegLo(Command.rt) ^ Section->MipsRegLo(Command.rs);
						Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_32;
					}
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_NOR: 
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt) && Section->IsConst(Command.rs)) {
					if (Section->Is64Bit(Command.rt) && Section->Is64Bit(Command.rs)) {
						Section->MipsReg(Command.rd) = ~(Section->MipsReg(Command.rt) | Section->MipsReg(Command.rs));
						Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_64;
					} else if (Section->Is64Bit(Command.rt) || Section->Is64Bit(Command.rs)) {
						if (Section->Is64Bit(Command.rt)) {
							Section->MipsReg(Command.rd) = ~(Section->MipsReg(Command.rt) | Section->MipsRegLo(Command.rs));
						} else {
							Section->MipsReg(Command.rd) = ~(Section->MipsRegLo(Command.rt) | Section->MipsReg(Command.rs));
						}
						Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_64;
					} else {
						Section->MipsRegLo(Command.rd) = ~(Section->MipsRegLo(Command.rt) | Section->MipsRegLo(Command.rs));
						Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_32;
					}
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SLT: 
				if (Command.rd == 0) { break; }
				if (Section->IsConst(Command.rt) && Section->IsConst(Command.rs)) {
					if (Section->Is64Bit(Command.rt) || Section->Is64Bit(Command.rs)) {
						if (Section->Is64Bit(Command.rt)) {
							Section->MipsRegLo(Command.rd) = (Section->MipsRegLo_S(Command.rs) < Section->MipsReg_S(Command.rt))?1:0;
						} else {
							Section->MipsRegLo(Command.rd) = (Section->MipsReg_S(Command.rs) < Section->MipsRegLo_S(Command.rt))?1:0;
						}
					} else {
						Section->MipsRegLo(Command.rd) = (Section->MipsRegLo_S(Command.rs) < Section->MipsRegLo_S(Command.rt))?1:0;
					}
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_32;
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SLTU: 
				if (Command.rd == 0) { break; }
				if (Section->IsConst(Command.rt) && Section->IsConst(Command.rs)) {
					if (Section->Is64Bit(Command.rt) || Section->Is64Bit(Command.rs)) {
						if (Section->Is64Bit(Command.rt)) {
							Section->MipsRegLo(Command.rd) = (Section->MipsRegLo(Command.rs) < Section->MipsReg(Command.rt))?1:0;
						} else {
							Section->MipsRegLo(Command.rd) = (Section->MipsReg(Command.rs) < Section->MipsRegLo(Command.rt))?1:0;
						}
					} else {
						Section->MipsRegLo(Command.rd) = (Section->MipsRegLo(Command.rs) < Section->MipsRegLo(Command.rt))?1:0;
					}
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_32;
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DADD: 
			case R4300i_SPECIAL_DADDU: 
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt) && Section->IsConst(Command.rs)) {
					Section->MipsReg(Command.rd) = 
						Section->Is64Bit(Command.rs)?Section->MipsReg(Command.rs):(_int64)Section->MipsRegLo_S(Command.rs) +
						Section->Is64Bit(Command.rt)?Section->MipsReg(Command.rt):(_int64)Section->MipsRegLo_S(Command.rt);
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_64;
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSUB: 
			case R4300i_SPECIAL_DSUBU: 
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt) && Section->IsConst(Command.rs)) {
					Section->MipsReg(Command.rd) = 
						Section->Is64Bit(Command.rs)?Section->MipsReg(Command.rs):(_int64)Section->MipsRegLo_S(Command.rs) -
						Section->Is64Bit(Command.rt)?Section->MipsReg(Command.rt):(_int64)Section->MipsRegLo_S(Command.rt);
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_64;
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSLL:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && Command.rt == Command.rd) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_64;
					Section->MipsReg(Command.rd) = Section->Is64Bit(Command.rt)?Section->MipsReg(Command.rt):(_int64)Section->MipsRegLo_S(Command.rt) << Command.sa;
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSRL:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && Command.rt == Command.rd) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_64;
					Section->MipsReg(Command.rd) = Section->Is64Bit(Command.rt)?Section->MipsReg(Command.rt):(QWORD)Section->MipsRegLo_S(Command.rt) >> Command.sa;
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSRA:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && Command.rt == Command.rd) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_64;
					Section->MipsReg_S(Command.rd) = Section->Is64Bit(Command.rt)?Section->MipsReg_S(Command.rt):(_int64)Section->MipsRegLo_S(Command.rt) >> Command.sa;
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSLL32:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && Command.rt == Command.rd) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_64;
					Section->MipsReg(Command.rd) = Section->MipsRegLo(Command.rt) << (Command.sa + 32);
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSRL32:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && Command.rt == Command.rd) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_32;
					Section->MipsRegLo(Command.rd) = (DWORD)(Section->MipsReg(Command.rt) >> (Command.sa + 32));
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSRA32:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && Command.rt == Command.rd) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;	
				}
				if (Section->IsConst(Command.rt)) {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_CONST_32;
					Section->MipsRegLo(Command.rd) = (DWORD)(Section->MipsReg_S(Command.rt) >> (Command.sa + 32));
				} else {
					Section->MipsRegState(Command.rd) = CRegInfo::STATE_UNKNOWN;
				}
				break;
			default:
#ifndef EXTERNAL_RELEASE
				if (Command.Hex == 0x00000001) { break; }
				DisplayError("Unhandled R4300i OpCode in FillSectionInfo 5\n%s",
					R4300iOpcodeName(Command.Hex,Section->CompilePC));
#endif
				NextInstruction = END_BLOCK;
				Section->CompilePC -= 4;
			}
			break;
		case R4300i_REGIMM:
			switch (Command.rt) {
			case R4300i_REGIMM_BLTZ:
			case R4300i_REGIMM_BGEZ:
				NextInstruction = DELAY_SLOT;
				Section->Cont.TargetPC = Section->CompilePC + 8;
				Section->Jump.TargetPC = Section->CompilePC + ((short)Command.offset << 2) + 4;
				if (Section->CompilePC == Section->Jump.TargetPC) {
					if (!DelaySlotEffectsCompare(Section->CompilePC,Command.rs,0)) {
						Section->Jump.PermLoop = true;
					}
				} 
				break;
			case R4300i_REGIMM_BLTZL:
			case R4300i_REGIMM_BGEZL:
				NextInstruction = LIKELY_DELAY_SLOT;
				Section->Cont.TargetPC = Section->CompilePC + 8;
				Section->Jump.TargetPC = Section->CompilePC + ((short)Command.offset << 2) + 4;
				if (Section->CompilePC == Section->Jump.TargetPC) { 
					if (!DelaySlotEffectsCompare(Section->CompilePC,Command.rs,0)) {
						Section->Jump.PermLoop = true;
					}
				} 
				break;
			case R4300i_REGIMM_BLTZAL:
				Section->MipsRegLo(31) = Section->CompilePC + 8;
				Section->MipsRegState(31) = CRegInfo::STATE_CONST_32;
				Section->Cont.TargetPC = Section->CompilePC + 8;
				Section->Jump.TargetPC = Section->CompilePC + ((short)Command.offset << 2) + 4;
				if (Section->CompilePC == Section->Jump.TargetPC) { 
					if (!DelaySlotEffectsCompare(Section->CompilePC,Command.rs,0)) {
						Section->Jump.PermLoop = true;
					}
				} 
				break;
			case R4300i_REGIMM_BGEZAL:
				NextInstruction = DELAY_SLOT;
				if (Section->IsConst(Command.rs)) 
				{
					__int64 Value;
					if (Section->Is32Bit(Command.rs))
					{
						Value = Section->MipsRegLo_S(Command.rs);
					} else {
						Value = Section->MipsReg_S(Command.rs);
					}
					if (Value >= 0) {
						Section->MipsRegLo(31) = Section->CompilePC + 8;
						Section->MipsRegState(31) = CRegInfo::STATE_CONST_32;
						Section->Jump.TargetPC = Section->CompilePC + ((short)Command.offset << 2) + 4;
						if (Section->CompilePC == Section->Jump.TargetPC) {
							if (!DelaySlotEffectsCompare(Section->CompilePC,31,0)) {
								Section->Jump.PermLoop = true;
							}
						} 
						break;
					}
				} 

				
				Section->MipsRegLo(31) = Section->CompilePC + 8;
				Section->MipsRegState(31) = CRegInfo::STATE_CONST_32;
				Section->Cont.TargetPC = Section->CompilePC + 8;
				Section->Jump.TargetPC = Section->CompilePC + ((short)Command.offset << 2) + 4;
				if (Section->CompilePC == Section->Jump.TargetPC) { 
					if (!DelaySlotEffectsCompare(Section->CompilePC,Command.rs,0)) {
						Section->Jump.PermLoop = true;
					}
				} 
				break;
			default:
#ifndef EXTERNAL_RELEASE
				if (Command.Hex == 0x0407000D) { break; }
				DisplayError("Unhandled R4300i OpCode in FillSectionInfo 4\n%s",
					R4300iOpcodeName(Command.Hex,Section->CompilePC));
#endif
				NextInstruction = END_BLOCK;
				Section->CompilePC -= 4;
			}
			break;
		case R4300i_JAL: 
			NextInstruction = DELAY_SLOT;
			Section->MipsRegLo(31) = Section->CompilePC + 8;
			Section->MipsRegState(31) = CRegInfo::STATE_CONST_32;
			Section->Jump.TargetPC = (Section->CompilePC & 0xF0000000) + (Command.target << 2);
			if (Section->CompilePC == Section->Jump.TargetPC) {
				if (!DelaySlotEffectsCompare(Section->CompilePC,31,0)) {
					Section->Jump.PermLoop = true;
				}
			} 
			break;
		case R4300i_J: 
			NextInstruction = DELAY_SLOT;
			Section->Jump.TargetPC = (Section->CompilePC & 0xF0000000) + (Command.target << 2);
			if (Section->CompilePC == Section->Jump.TargetPC) { Section->Jump.PermLoop = true; } 
			break;
		case R4300i_BEQ: 
			NextInstruction = DELAY_SLOT;
			Section->Cont.TargetPC = Section->CompilePC + 8;
			Section->Jump.TargetPC = Section->CompilePC + ((short)Command.offset << 2) + 4;
			if (Section->CompilePC == Section->Jump.TargetPC) {
				if (!DelaySlotEffectsCompare(Section->CompilePC,Command.rs,Command.rt)) {
					Section->Jump.PermLoop = true;
				}
			} 
			if (Section->IsConst(Command.rs) && Section->IsConst(Command.rt)) 
			{
				__int64 Value1, Value2;
				if (Section->Is32Bit(Command.rs))
				{
					Value1 = Section->MipsRegLo_S(Command.rs);
				} else {
					Value1 = Section->MipsReg_S(Command.rs);
				}
				if (Section->Is32Bit(Command.rt))
				{
					Value2 = Section->MipsRegLo_S(Command.rt);
				} else {
					Value2 = Section->MipsReg_S(Command.rt);
				}
				if (Value1 == Value2) 
				{
					Section->Cont.TargetPC = -1;
				}
			} 
			break;
		case R4300i_BNE: 
		case R4300i_BLEZ: 
		case R4300i_BGTZ: 
			NextInstruction = DELAY_SLOT;
			Section->Cont.TargetPC = Section->CompilePC + 8;
			Section->Jump.TargetPC = Section->CompilePC + ((short)Command.offset << 2) + 4;
			if (Section->CompilePC == Section->Jump.TargetPC) {
				if (!DelaySlotEffectsCompare(Section->CompilePC,Command.rs,Command.rt)) {
					Section->Jump.PermLoop = true;
				}
			} 
			break;
		case R4300i_ADDI: 
		case R4300i_ADDIU: 
			if (Command.rt == 0) { break; }
			if (Section->InLoop && Command.rs == Command.rt) {
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_UNKNOWN;	
			}
			if (Section->IsConst(Command.rs)) { 
				Section->MipsRegLo(Command.rt) = Section->MipsRegLo(Command.rs) + (short)Command.immediate;
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_CONST_32;
			} else {
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_UNKNOWN;
			}
			break;
		case R4300i_SLTI: 
			if (Command.rt == 0) { break; }
			if (Section->IsConst(Command.rs)) { 
				if (Section->Is64Bit(Command.rs)) {
					Section->MipsRegLo(Command.rt) = (Section->MipsReg_S(Command.rs) < (_int64)((short)Command.immediate))?1:0;
				} else {
					Section->MipsRegLo(Command.rt) = (Section->MipsRegLo_S(Command.rs) < (int)((short)Command.immediate))?1:0;
				}
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_CONST_32;
			} else {
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_UNKNOWN;
			}
			break;
		case R4300i_SLTIU: 
			if (Command.rt == 0) { break; }
			if (Section->IsConst(Command.rs)) { 
				if (Section->Is64Bit(Command.rs)) {
					Section->MipsRegLo(Command.rt) = (Section->MipsReg(Command.rs) < (unsigned _int64)((short)Command.immediate))?1:0;
				} else {
					Section->MipsRegLo(Command.rt) = (Section->MipsRegLo(Command.rs) < (DWORD)((short)Command.immediate))?1:0;
				}
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_CONST_32;
			} else {
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_UNKNOWN;
			}
			break;
		case R4300i_LUI: 
			if (Command.rt == 0) { break; }
			Section->MipsRegLo(Command.rt) = ((short)Command.offset << 16);
			Section->MipsRegState(Command.rt) = CRegInfo::STATE_CONST_32;
			break;
		case R4300i_ANDI: 
			if (Command.rt == 0) { break; }
			if (Section->InLoop && Command.rs == Command.rt) {
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_UNKNOWN;	
			}
			if (Section->IsConst(Command.rs)) {
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_CONST_32;
				Section->MipsRegLo(Command.rt) = Section->MipsRegLo(Command.rs) & Command.immediate;
			} else {
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_UNKNOWN;
			}
			break;
		case R4300i_ORI: 
			if (Command.rt == 0) { break; }
			if (Section->InLoop && Command.rs == Command.rt) {
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_UNKNOWN;	
			}
			if (Section->IsConst(Command.rs)) {
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_CONST_32;
				Section->MipsRegLo(Command.rt) = Section->MipsRegLo(Command.rs) | Command.immediate;
			} else {
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_UNKNOWN;
			}
			break;
		case R4300i_XORI: 
			if (Command.rt == 0) { break; }
			if (Section->InLoop && Command.rs == Command.rt) {
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_UNKNOWN;	
			}
			if (Section->IsConst(Command.rs)) {
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_CONST_32;
				Section->MipsRegLo(Command.rt) = Section->MipsRegLo(Command.rs) ^ Command.immediate;
			} else {
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_UNKNOWN;
			}
			break;
		case R4300i_CP0:
			switch (Command.rs) {
			case R4300i_COP0_MF:
				if (Command.rt == 0) { break; }
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_UNKNOWN;
				break;
			case R4300i_COP0_MT: break;
			default:
				if ( (Command.rs & 0x10 ) != 0 ) {
					switch( Command.funct ) {
					case R4300i_COP0_CO_TLBR: break;
					case R4300i_COP0_CO_TLBWI: break;
					case R4300i_COP0_CO_TLBWR: break;
					case R4300i_COP0_CO_TLBP: break;
					case R4300i_COP0_CO_ERET: NextInstruction = END_BLOCK; break;
					default:
#ifndef EXTERNAL_RELEASE
						DisplayError("Unhandled R4300i OpCode in FillSectionInfo\n%s",
							R4300iOpcodeName(Command.Hex,Section->CompilePC));
#endif
						NextInstruction = END_BLOCK;
						Section->CompilePC -= 4;
					}
				} else {
#ifndef EXTERNAL_RELEASE
					DisplayError("Unhandled R4300i OpCode in FillSectionInfo 3\n%s",
						R4300iOpcodeName(Command.Hex,Section->CompilePC));
#endif
					NextInstruction = END_BLOCK;
					Section->CompilePC -= 4;
				}
			}
			break;
		case R4300i_CP1:
			switch (Command.fmt) {
			case R4300i_COP1_CF:
			case R4300i_COP1_MF:
			case R4300i_COP1_DMF:
				if (Command.rt == 0) { break; }
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_UNKNOWN;
				break;
			case R4300i_COP1_BC:
				switch (Command.ft) {
				case R4300i_COP1_BC_BCFL:
				case R4300i_COP1_BC_BCTL:
					NextInstruction = LIKELY_DELAY_SLOT;
					Section->Cont.TargetPC = Section->CompilePC + 8;
					Section->Jump.TargetPC = Section->CompilePC + ((short)Command.offset << 2) + 4;
					if (Section->CompilePC == Section->Jump.TargetPC) {
						int EffectDelaySlot;
						OPCODE NewCommand;

						if (!r4300i_LW_VAddr(Section->CompilePC + 4, &NewCommand.Hex)) {
							DisplayError(GS(MSG_FAIL_LOAD_WORD));
							ExitThread(0);
						}
						
						EffectDelaySlot = false;
						if (NewCommand.op == R4300i_CP1) {
							if (NewCommand.fmt == R4300i_COP1_S && (NewCommand.funct & 0x30) == 0x30 ) {
								EffectDelaySlot = true;
							} 
							if (NewCommand.fmt == R4300i_COP1_D && (NewCommand.funct & 0x30) == 0x30 ) {
								EffectDelaySlot = true;
							} 
						}						
						if (!EffectDelaySlot) {
							Section->Jump.PermLoop = true;
						}
					} 
					break;
				case R4300i_COP1_BC_BCF:
				case R4300i_COP1_BC_BCT:
					NextInstruction = DELAY_SLOT;
					Section->Cont.TargetPC = Section->CompilePC + 8;
					Section->Jump.TargetPC = Section->CompilePC + ((short)Command.offset << 2) + 4;
					if (Section->CompilePC == Section->Jump.TargetPC) {
						int EffectDelaySlot;
						OPCODE NewCommand;

						if (!r4300i_LW_VAddr(Section->CompilePC + 4, &NewCommand.Hex)) {
							DisplayError(GS(MSG_FAIL_LOAD_WORD));
							ExitThread(0);
						}
						
						EffectDelaySlot = false;
						if (NewCommand.op == R4300i_CP1) {
							if (NewCommand.fmt == R4300i_COP1_S && (NewCommand.funct & 0x30) == 0x30 ) {
								EffectDelaySlot = true;
							} 
							if (NewCommand.fmt == R4300i_COP1_D && (NewCommand.funct & 0x30) == 0x30 ) {
								EffectDelaySlot = true;
							} 
						}						
						if (!EffectDelaySlot) {
							Section->Jump.PermLoop = true;
						}
					} 
					break;
				}
				break;
			case R4300i_COP1_MT: break;
			case R4300i_COP1_DMT: break;
			case R4300i_COP1_CT: break;
			case R4300i_COP1_S: break;
			case R4300i_COP1_D: break;
			case R4300i_COP1_W: break;
			case R4300i_COP1_L: break;
			default:
#ifndef EXTERNAL_RELEASE
				DisplayError("Unhandled R4300i OpCode in FillSectionInfo 2\n%s",
					R4300iOpcodeName(Command.Hex,Section->CompilePC));
#endif
				NextInstruction = END_BLOCK;
				Section->CompilePC -= 4;
			}
			break;
		case R4300i_BEQL: 
		case R4300i_BNEL: 
		case R4300i_BLEZL: 
		case R4300i_BGTZL: 
			NextInstruction = LIKELY_DELAY_SLOT;
			Section->Cont.TargetPC = Section->CompilePC + 8;
			Section->Jump.TargetPC = Section->CompilePC + ((short)Command.offset << 2) + 4;
			if (Section->CompilePC == Section->Jump.TargetPC) {
				if (!DelaySlotEffectsCompare(Section->CompilePC,Command.rs,Command.rt)) {
					Section->Jump.PermLoop = true;
				}
			} 
			break;
		case R4300i_DADDI: 
		case R4300i_DADDIU: 
			if (Command.rt == 0) { break; }
			if (Section->InLoop && Command.rs == Command.rt) {
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_UNKNOWN;	
			}
			if (Section->IsConst(Command.rs)) { 
				if (Section->Is64Bit(Command.rs)) { 
					int imm32 = (short)Opcode.immediate;
					__int64 imm64 = imm32;										
					Section->MipsReg_S(Command.rt) = Section->MipsRegLo_S(Command.rs) + imm64;
				} else {
					Section->MipsReg_S(Command.rt) = Section->MipsRegLo_S(Command.rs) + (short)Command.immediate;
				}
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_CONST_64;
			} else {
				Section->MipsRegState(Command.rt) = CRegInfo::STATE_UNKNOWN;
			}
			break;
		case R4300i_LDR:
		case R4300i_LDL:
		case R4300i_LB:
		case R4300i_LH: 
		case R4300i_LWL: 
		case R4300i_LW: 
		case R4300i_LWU: 
		case R4300i_LL: 
		case R4300i_LBU:
		case R4300i_LHU: 
		case R4300i_LWR: 
		case R4300i_SC: 
			if (Command.rt == 0) { break; }
			Section->MipsRegState(Command.rt) = CRegInfo::STATE_UNKNOWN;
			break;
		case R4300i_SB: break;
		case R4300i_SH: break;
		case R4300i_SWL: break;
		case R4300i_SW: break;
		case R4300i_SWR: break;
		case R4300i_SDL: break;
		case R4300i_SDR: break;
		case R4300i_CACHE: break;
		case R4300i_LWC1: break;
		case R4300i_SWC1: break;
		case R4300i_LDC1: break;
		case R4300i_LD:
			if (Command.rt == 0) { break; }
			Section->MipsRegState(Command.rt) = CRegInfo::STATE_UNKNOWN;
			break;
		case R4300i_SDC1: break;
		case R4300i_SD: break;
		default:
			NextInstruction = END_BLOCK;
			Section->CompilePC -= 4;
			if (Command.Hex == 0x7C1C97C0) { break; }
			if (Command.Hex == 0x7FFFFFFF) { break; }
			if (Command.Hex == 0xF1F3F5F7) { break; }
			if (Command.Hex == 0xC1200000) { break; }
			if (Command.Hex == 0x4C5A5353) { break; }
#ifndef EXTERNAL_RELEASE
			DisplayError("Unhandled R4300i OpCode in FillSectionInfo 1\n%s\n%X",
				R4300iOpcodeName(Command.Hex,Section->CompilePC),Command.Hex);
#endif
		}

//		if (Section->CompilePC == 0x8005E4B8) {
//CPU_Message("%X: %s %s = %d",Section->CompilePC,R4300iOpcodeName(Command.Hex,Section->CompilePC),
//			GPR_Name[8],Section->MipsRegState(8));
//_asm int 3
//		}
		switch (NextInstruction) {
		case NORMAL: 
			Section->CompilePC += 4; 
			break;
		case DELAY_SLOT:
			NextInstruction = DELAY_SLOT_DONE;
			Section->CompilePC += 4; 
			break;
		case LIKELY_DELAY_SLOT:
			if (Section->Cont.TargetPC == Section->Jump.TargetPC)
			{
				Section->Jump.RegSet = Section->RegWorking; 
				Section->Cont.DoneDelaySlot = false;
				Section->Cont.RegSet = Section->RegWorking;
				Section->Cont.DoneDelaySlot = true;
				NextInstruction = END_BLOCK;
			} else {
				Section->Cont.RegSet = Section->RegWorking;
				Section->Cont.DoneDelaySlot = true;
				NextInstruction = LIKELY_DELAY_SLOT_DONE;
				Section->CompilePC += 4; 
			}
			break;
		case DELAY_SLOT_DONE:
			Section->Cont.RegSet = Section->RegWorking;
			Section->Jump.RegSet = Section->RegWorking; 
			Section->Cont.DoneDelaySlot = true;
			Section->Jump.DoneDelaySlot = true; 
			NextInstruction = END_BLOCK;
			break;
		case LIKELY_DELAY_SLOT_DONE:
			Section->Jump.RegSet = Section->RegWorking;
			Section->Jump.DoneDelaySlot = true; 
			NextInstruction = END_BLOCK;
			break;
		}		
		if ((Section->CompilePC & 0xFFFFF000) != (Section->StartPC & 0xFFFFF000)) {
			if (NextInstruction != END_BLOCK && NextInstruction != NORMAL) {
			//	DisplayError("Branch running over delay slot ???\nNextInstruction == %d",NextInstruction);
				Section->Cont.TargetPC = (DWORD)-1;
				Section->Jump.TargetPC = (DWORD)-1;
			} 
			NextInstruction = END_BLOCK;
			Section->CompilePC -= 4;
		}
	} while (NextInstruction != END_BLOCK);

	if (Section->Cont.TargetPC != (DWORD)-1) {
		if ((Section->Cont.TargetPC & 0xFFFFF000) != (Section->StartPC & 0xFFFFF000)) {
			Section->Cont.TargetPC = (DWORD)-1;
		}
	}
	if (Section->Jump.TargetPC != (DWORD)-1) {
		if (Section->Jump.TargetPC < Section->BlockInfo->StartVAddr)
		{
			Section->Jump.TargetPC = (DWORD)-1;
		}
		if ((Section->Jump.TargetPC & 0xFFFFF000) != (Section->StartPC & 0xFFFFF000)) {
			Section->Jump.TargetPC = (DWORD)-1;
		}
	}
	return true;
}

void CRecompiler::CompileExitCode ( CBlockInfo & BlockInfo )
{
	for (EXIT_LIST::iterator ExitIter = BlockInfo.ExitInfo.begin(); ExitIter != BlockInfo.ExitInfo.end(); ExitIter++)
	{
		CPU_Message("");
		CPU_Message("      $Exit_%d",ExitIter->ID);
		SetJump32(ExitIter->JumpLoc,RecompPos);	
		NextInstruction = ExitIter->NextInstruction;
		CompileExit(&BlockInfo.ParentSection, -1, ExitIter->TargetPC,ExitIter->ExitRegSet,ExitIter->reason,true,NULL);
	}
}

bool CRecompiler::Compiler4300iBlock(FUNCTION_INFO * info) {
	DWORD StartTime = timeGetTime();
	WriteTraceF(TraceRecompiler,"Compile Block-Start: %X-%X",info->VStartPC(),info->VEndPC());
	
	if (bProfiling())    { m_Profile.StartTimer(Timer_GetBlockInfo); }
	
	CBlockInfo BlockInfo(PROGRAM_COUNTER, RecompPos);
	if (bProfiling())    { m_Profile.StartTimer(Timer_AnalyseBlock); }
	if (!AnalyseBlock(BlockInfo)) { return false; }

	if (bProfiling())    { m_Profile.StartTimer(Timer_CompileBlock); }
	DWORD StartAddress;
	if (!_MMU->TranslateVaddr(BlockInfo.StartVAddr,StartAddress))
	{
		DisplayError("Ummm... Where does this block go\n%X",BlockInfo.StartVAddr);
		return false;
	}

/*	MarkCodeBlock(StartAddress);
	if (StartAddress < RdramSize()) {
		CPU_Message("====== RDRAM: block (%X:%d) ======", StartAddress>>12,N64_Blocks.NoOfRDRamBlocks[StartAddress>>12]);
	} else if (StartAddress >= 0x04000000 && StartAddress <= 0x04000FFC) {
		CPU_Message("====== DMEM: block (%d) ======", N64_Blocks.NoOfDMEMBlocks);
	} else if (StartAddress >= 0x04001000 && StartAddress <= 0x04001FFC) {
		CPU_Message("====== IMEM: block (%d) ======", N64_Blocks.NoOfIMEMBlocks);
	} else if (StartAddress >= 0x1FC00000 && StartAddress <= 0x1FC00800) {
		CPU_Message("====== PIF ROM: block ======");
	} else {
#ifndef ROM_IN_MAPSPACE
#ifndef EXTERNAL_RELEASE
		DisplayError("Ummm... Where does this block go");
#endif
		ExitThread(0);			
#endif
	}*/
	CPU_Message("x86 code at: %X",BlockInfo.CompiledLocation);
	CPU_Message("Start of Block: %X",BlockInfo.StartVAddr );
	CPU_Message("No of Sections: %d",BlockInfo.NoOfSections );
	CPU_Message("====== recompiled code ======");
	if (bLinkBlocks()) {
		for (int count = 0; count < BlockInfo.NoOfSections; count ++) {
			DisplaySectionInformation(&BlockInfo.ParentSection,count + 1,CBlockSection::GetNewTestValue());
		}
	}
	if (m_SyncSystem) {
		//if ((DWORD)BlockInfo.CompiledLocation == 0x60A7B73B) { X86BreakPoint(__FILE__,__LINE__); }
		//MoveConstToVariable((DWORD)BlockInfo.CompiledLocation,&CurrentBlock,"CurrentBlock");
	}
	
	BlockInfo.ParentSection.RegStart.Initilize();
	BlockInfo.ParentSection.RegWorking = BlockInfo.ParentSection.RegStart;
	if (bLinkBlocks()) {
		while (GenerateX86Code(BlockInfo,&BlockInfo.ParentSection,CBlockSection::GetNewTestValue()));
	} else {
		GenerateX86Code(BlockInfo,&BlockInfo.ParentSection,CBlockSection::GetNewTestValue());
	}
	CompileExitCode(BlockInfo);
	
	CPU_Message("====== End of recompiled code ======");
	if (bProfiling())    { m_Profile.StartTimer(Timer_CompileDone); }

	info->SetVEndPC(BlockInfo.EndVAddr);
	info->SetFunctionAddr(BlockInfo.CompiledLocation);
	_MMU->VAddrToRealAddr(info->VStartPC(),*(reinterpret_cast<void **>(&info->MemLocation[0])));
	info->MemLocation[1] = info->MemLocation[0] + 1;
	info->MemContents[0] = *info->MemLocation[0];
	info->MemContents[1] = *info->MemLocation[1];
	if (bSMM_Protect())
	{
		_MMU->ProtectMemory(info->VStartPC(),info->VEndPC());
	}
	NextInstruction = NORMAL;

	if (bShowRecompMemSize()) 
	{
		DWORD Size = RecompPos - RecompCode;
		DWORD MB = Size / 0x100000;
		Size -= MB * 0x100000;
		DWORD KB = Size / 1024;
		Size -= KB  * 1024;

		DWORD TotalAvaliable = _MMU->GetRecompBufferSize() / 0x100000;
		
		DisplayMessage(0,"Memory used: %d mb %-3d kb %-3d bytes     Total Available: %d mb",MB,KB,Size, TotalAvaliable);
	}
	if (bProfiling())    { m_Profile.StopTimer(); }

	DWORD TimeTaken = timeGetTime() - StartTime;
	WriteTraceF(TraceRecompiler,"Compile Block-Done: %X-%X  - Taken: %d",info->VStartPC(),info->VEndPC(),TimeTaken);
	return true;
}


void CRecompiler::RecompilerMain_ChangeMemory ( void )
{
#ifdef tofix
	DWORD Value, Addr;
	BYTE * Block;

	while(!EndEmulation()) {
		if (UseTlb) {
			if (!TranslateVaddr(PROGRAM_COUNTER, &Addr)) {
				DoTLBMiss(NextInstruction == DELAY_SLOT,PROGRAM_COUNTER);
				NextInstruction = NORMAL;
				if (!TranslateVaddr(PROGRAM_COUNTER, &Addr)) {
#ifndef EXTERNAL_RELEASE
					DisplayError("Failed to tranlate PC to a PAddr: %X\n\nEmulation stopped",PROGRAM_COUNTER);
#endif
					ExitThread(0);
				}
			}
		} else {
			Addr = PROGRAM_COUNTER & 0x1FFFFFFF;
		}

		if (NextInstruction == DELAY_SLOT) {
			__try {
				Value = (DWORD)(*(DelaySlotTable + (Addr >> 12)));
			} __except(EXCEPTION_EXECUTE_HANDLER) {
#ifndef EXTERNAL_RELEASE
				DisplayError("Executing Delay Slot from non maped space\nPROGRAM_COUNTER = 0x%X",PROGRAM_COUNTER);
#endif
				ExitThread(0);
			}
			if ( (Value >> 16) == 0x7C7C) {
				DWORD Index = (Value & 0xFFFF);
				Block = (BYTE *)OrigMem[Index].CompiledLocation;
				if (OrigMem[Index].PAddr != Addr) { Block = NULL; }
				if (OrigMem[Index].VAddr != PROGRAM_COUNTER) { Block = NULL; }
				if (Index >= TargetIndex) { Block = NULL; }
			} else {
				Block = NULL;
			}						
			if (Block == NULL) {
				DWORD MemValue;

				Block = CompileDelaySlot();
				Value = 0x7C7C0000;
				Value += (WORD)(TargetIndex);
				MemValue = *(DWORD *)(N64MEM + Addr);
				if ( (MemValue >> 16) == 0x7C7C) {
					MemValue = OrigMem[(MemValue & 0xFFFF)].OriginalValue;
				}
				OrigMem[(WORD)(TargetIndex)].OriginalValue = MemValue;
				OrigMem[(WORD)(TargetIndex)].CompiledLocation = Block;
				OrigMem[(WORD)(TargetIndex)].PAddr = Addr;
				OrigMem[(WORD)(TargetIndex)].VAddr = PROGRAM_COUNTER;
				TargetIndex += 1;
				*(DelaySlotTable + (Addr >> 12)) = (void *)Value;
				NextInstruction = NORMAL;
			}
			_asm {
				pushad
				call Block
				popad
			}
			continue;
		}

		__try {
			Value = *(DWORD *)(N64MEM + Addr);
			if ( (Value >> 16) == 0x7C7C) {
				DWORD Index = (Value & 0xFFFF);
				Block = (BYTE *)OrigMem[Index].CompiledLocation;						
				if (OrigMem[Index].PAddr != Addr) { Block = NULL; }
				if (OrigMem[Index].VAddr != PROGRAM_COUNTER) { Block = NULL; }
				if (Index >= TargetIndex) { Block = NULL; }
			} else {
				Block = NULL;
			}
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			DisplayError(GS(MSG_NONMAPPED_SPACE));
			ExitThread(0);
		}
						
		if (Block == NULL) {
			DWORD MemValue;

			__try {
				Block = Compiler4300iBlock();
			} __except(EXCEPTION_EXECUTE_HANDLER) {
				ResetRecompCode();
				Block = Compiler4300iBlock();
			}
			if (EndEmulation())
			{
				continue;
			}
			if (TargetIndex == MaxOrigMem) {
				ResetRecompCode();
				continue;
			}
			Value = 0x7C7C0000;
			Value += (WORD)(TargetIndex);
			MemValue = *(DWORD *)(N64MEM + Addr);
			if ( (MemValue >> 16) == 0x7C7C) {
				MemValue = OrigMem[(MemValue & 0xFFFF)].OriginalValue;
			}
			OrigMem[(WORD)(TargetIndex)].OriginalValue = MemValue;
			OrigMem[(WORD)(TargetIndex)].CompiledLocation = Block;
			OrigMem[(WORD)(TargetIndex)].PAddr = Addr;					
			OrigMem[(WORD)(TargetIndex)].VAddr = PROGRAM_COUNTER;
			TargetIndex += 1;
			*(DWORD *)(N64MEM + Addr) = Value;					
			NextInstruction = NORMAL;
		}
		if (Profiling && IndvidualBlock) {
			static DWORD ProfAddress = 0;

			/*if ((PROGRAM_COUNTER & ~0xFFF) != ProfAddress) {
				char Label[100];

				ProfAddress = PROGRAM_COUNTER & ~0xFFF;
				sprintf(Label,"PC: %X to %X",ProfAddress,ProfAddress+ 0xFFC);
				StartTimer(Label);				
			}*/
			/*if (PROGRAM_COUNTER >= 0x800DD000 && PROGRAM_COUNTER <= 0x800DDFFC) {
				char Label[100];
				sprintf(Label,"PC: %X   Block: %X",PROGRAM_COUNTER,Block);
				StartTimer(Label);				
			}*/
//				} else 	if ((Profiling || ShowCPUPer) && ProfilingLabel[0] == 0) { 
//					StartTimer("r4300i Running"); 
		}
		_asm {
			pushad
			call Block
			popad
		}
	} // end for(;;)
#endif
}

FUNCTION_INFO * CRecompiler::CompilerCode ( void )
{
	_MMU->CheckRecompMem(RecompPos);
	FUNCTION_INFO * Info = m_Functions.AddFunctionInfo(PROGRAM_COUNTER,_MMU->TranslateVaddr(PROGRAM_COUNTER));
	__try {
		if (!Compiler4300iBlock(Info))
		{
			return NULL;
		}
		return Info;
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		ResetRecompCode();
		Info = m_Functions.AddFunctionInfo(PROGRAM_COUNTER,_MMU->TranslateVaddr(PROGRAM_COUNTER));
		if (!Compiler4300iBlock(Info))
		{
			return NULL;
		}
		return Info;
	}
	return NULL;
}

void CRecompiler::DetermineLoop(CBlockSection * Section, DWORD Test, DWORD Test2, DWORD TestID) 
{
	if (Section == NULL) { return; }
	if (Section->SectionID != TestID) {
		if (Section->Test2 == Test2) {
			return; 
		}
		Section->Test2 = Test2;
		DetermineLoop(Section->ContinueSection,Test,Test2,TestID);
		DetermineLoop(Section->JumpSection,Test,Test2,TestID);
		return;
	}
	if (Section->Test2 == Test2) { 
		Section->InLoop = true;
		return; 
	}
	Section->Test2 = Test2;
	DetermineLoop(Section->ContinueSection,Test,Test2,TestID);
	DetermineLoop(Section->JumpSection,Test,Test2,TestID);
	if (Section->Test == Test) { return; }
	Section->Test = Test;
	if (Section->ContinueSection != NULL) {
		DetermineLoop(Section->ContinueSection,Test,CBlockSection::GetNewTestValue(),Section->ContinueSection->SectionID);
	}
	if (Section->JumpSection != NULL) {
		DetermineLoop(Section->JumpSection,Test,CBlockSection::GetNewTestValue(),Section->JumpSection->SectionID);
	}
}

bool CRecompiler::DisplaySectionInformation (CBlockSection * Section, DWORD ID, DWORD Test)
{
	if (!IsX86Logging())
	{
		return false;
	}
	if (Section == NULL) { return false; }
	if (Section->Test == Test) { return false; }
	Section->Test = Test;
	if (Section->SectionID != ID) {
		if (DisplaySectionInformation(Section->ContinueSection,ID,Test)) { return true; }
		if (DisplaySectionInformation(Section->JumpSection,ID,Test)) { return true; }
		return false;
	}
	CPU_Message("====== Section %d ======",Section->SectionID);
	CPU_Message("Start PC: %X",Section->StartPC);
	CPU_Message("CompiledLocation: %X",RecompPos);
	if (!Section->ParentSection.empty()) 
	{
		stdstr ParentList;
		for (SECTION_LIST::iterator iter = Section->ParentSection.begin(); iter != Section->ParentSection.end(); iter++)
		{
			CBlockSection * Parent = *iter;
			if (!ParentList.empty())
			{
				ParentList += ", ";
			}
			ParentList += stdstr_f("%d",Parent->SectionID);
		}
		CPU_Message("Number of parents: %d (%s)",Section->ParentSection.size(),ParentList.c_str());
	}

	if (Section->JumpSection != NULL) {
		CPU_Message("Jump Section: %d",Section->JumpSection->SectionID);
	} else {
		CPU_Message("Jump Section: None");
	}
	if (Section->ContinueSection != NULL) {
		CPU_Message("Continue Section: %d",Section->ContinueSection->SectionID);
	} else {
		CPU_Message("Continue Section: None");
	}
	CPU_Message("=======================",Section->SectionID);
	return true;
}

bool CRecompiler::InheritParentInfo (CBlockSection * Section)
{	
	/*	int count, start, NoOfParents, NoOfCompiledParents, FirstParent,CurrentParent;
	BLOCK_PARENT * SectionParents;
	BLOCK_SECTION * Parent;
	JUMP_INFO * JumpInfo;
	char Label[100];
	BOOL NeedSync;
*/
	DisplaySectionInformation(Section,Section->SectionID,CBlockSection::GetNewTestValue());

	if (Section->ParentSection.empty()) 
	{
		Section->RegStart.Initilize();
		Section->RegWorking = Section->RegStart;		
		return true;
	} 

	if (Section->ParentSection.size() == 1) 
	{ 
		CBlockSection * Parent = *(Section->ParentSection.begin());
		CJumpInfo * JumpInfo = Section == Parent->ContinueSection ? &Parent->Cont : &Parent->Jump;

		Section->RegStart = JumpInfo->RegSet;
		if (JumpInfo->LinkLocation != NULL) {
			CPU_Message("   Section_%d:",Section->SectionID);
			SetJump32(JumpInfo->LinkLocation,RecompPos);
			if (JumpInfo->LinkLocation2 != NULL) { 
				SetJump32(JumpInfo->LinkLocation2,RecompPos);
			}
		}
		Section->RegWorking = Section->RegStart;
		return true;
	}

	//Multiple Parents
	BLOCK_PARENT_LIST ParentList;
	SECTION_LIST::iterator iter;
	for (iter = Section->ParentSection.begin(); iter != Section->ParentSection.end(); iter++)
	{
		CBlockSection * Parent = *iter;
		BLOCK_PARENT BlockParent;

		if (Parent->CompiledLocation == NULL) { continue; }
		if (Parent->JumpSection != Parent->ContinueSection) {
			BlockParent.Parent = Parent;
			BlockParent.JumpInfo = Section == Parent->ContinueSection?&Parent->Cont:&Parent->Jump;
			ParentList.push_back(BlockParent);
		} else {
			BlockParent.Parent = Parent;
			BlockParent.JumpInfo = &Parent->Cont;
			ParentList.push_back(BlockParent);
			BlockParent.Parent = Parent;
			BlockParent.JumpInfo = &Parent->Jump;
			ParentList.push_back(BlockParent);
		}
	}
	int NoOfCompiledParents = ParentList.size();
	if (NoOfCompiledParents == 0)
	{
		DisplayError("No Parent has been compiled ????"); 
		return false; 
	}	

	// Add all the uncompiled blocks to the end of the list
	for (iter = Section->ParentSection.begin(); iter != Section->ParentSection.end(); iter++)
	{
		CBlockSection * Parent = *iter;
		BLOCK_PARENT BlockParent;

		if (Parent->CompiledLocation != NULL) { continue; }
		if (Parent->JumpSection != Parent->ContinueSection) {
			BlockParent.Parent = Parent;
			BlockParent.JumpInfo = Section == Parent->ContinueSection?&Parent->Cont:&Parent->Jump;
			ParentList.push_back(BlockParent);
		} else {
			BlockParent.Parent = Parent;
			BlockParent.JumpInfo = &Parent->Cont;
			ParentList.push_back(BlockParent);
			BlockParent.Parent = Parent;
			BlockParent.JumpInfo = &Parent->Jump;
			ParentList.push_back(BlockParent);
		}
	}
	int FirstParent = 0;
	int count = 1;
	for (;count < NoOfCompiledParents;count++) {
		if (ParentList[count].JumpInfo->FallThrough) {
			FirstParent = count; break;
		}
	}

	//Link First Parent to start
	CBlockSection * Parent = ParentList[FirstParent].Parent;
	CJumpInfo * JumpInfo = ParentList[FirstParent].JumpInfo;

	Section->RegWorking = JumpInfo->RegSet;
	if (JumpInfo->LinkLocation != NULL) {
		CPU_Message("   Section_%d (from %d):",Section->SectionID,Parent->SectionID);
		SetJump32(JumpInfo->LinkLocation,RecompPos);
		JumpInfo->LinkLocation  = NULL;
		if (JumpInfo->LinkLocation2 != NULL) { 
			SetJump32(JumpInfo->LinkLocation2,RecompPos);
			JumpInfo->LinkLocation2  = NULL;
		}
	}

	if (Section->StartPC < Parent->CompilePC )
	{
		UpdateCounters(&JumpInfo->RegSet.BlockCycleCount(),&JumpInfo->RegSet.BlockRandomModifier(),true);
		CompileSystemCheck(Section->StartPC,JumpInfo->RegSet);
	} else {
		UpdateCounters(&JumpInfo->RegSet.BlockCycleCount(),&JumpInfo->RegSet.BlockRandomModifier(),false);
	}
	JumpInfo->FallThrough   = false;

	//Fix up initial state
	UnMap_AllFPRs(Section);
	for (count = 0;count < ParentList.size();count++) {
		int count2, MemoryStackPos;

		if (count == FirstParent) { continue; }		
		Parent = ParentList[count].Parent;
		CRegInfo * RegSet = &ParentList[count].JumpInfo->RegSet;
			
		if (Section->CurrentRoundingModel() != RegSet->CurrentRoundingModel()) { Section->CurrentRoundingModel() = CRegInfo::RoundUnknown; }
		if (ParentList.size() != NoOfCompiledParents) { Section->CurrentRoundingModel() = CRegInfo::RoundUnknown; }

		//Find Parent MapRegState
		MemoryStackPos = -1;
		for (count2 = 1; count2 < 10; count2++) {
			if (RegSet->x86Mapped(count2) == CRegInfo::Stack_Mapped) {
				MemoryStackPos = count2;
				break;
			}
		}
		if (MemoryStackPos < 0) {
			// if the memory stack position is not mapped then unmap it
			int MemStackReg = Map_MemoryStack(Section,x86_Any,false); 
			if (MemStackReg > 0) 
			{
				UnMap_X86reg(Section,MemStackReg);
			}
		}


		for (count2 = 1; count2 < 32; count2++) {
			if (Section->Is32BitMapped(count2)) {
				switch (RegSet->MipsRegState(count2)) {
				case CRegInfo::STATE_MAPPED_64: Map_GPR_64bit(Section,count2,count2); break;
				case CRegInfo::STATE_MAPPED_32_ZERO: break;
				case CRegInfo::STATE_MAPPED_32_SIGN:
					if (Section->IsUnsigned(count2)) {
						Section->MipsRegState(count2) = CRegInfo::STATE_MAPPED_32_SIGN;
					}
					break;
				case CRegInfo::STATE_CONST_64: Map_GPR_64bit(Section,count2,count2); break;
				case CRegInfo::STATE_CONST_32: 
					if ((RegSet->MipsRegLo_S(count2) < 0) && Section->IsUnsigned(count2)) {
						Section->MipsRegState(count2) = CRegInfo::STATE_MAPPED_32_SIGN;
					}
					break;
				case CRegInfo::STATE_UNKNOWN:
					//Map_GPR_32bit(Section,count2,true,count2);
					Map_GPR_64bit(Section,count2,count2); //??
					//UnMap_GPR(Section,count2,true); ??
					break;
#ifndef EXTERNAL_RELEASE
				default:
					DisplayError("Unknown CPU State(%d) in InheritParentInfo",RegSet->MipsRegState(count2));
#endif
				}
			}
			if (Section->IsConst(count2)) {
				if (Section->MipsRegState(count2) != RegSet->MipsRegState(count2)) {
					if (Section->Is32Bit(count2)) {
						Map_GPR_32bit(Section,count2,true,count2);
					} else {
						Map_GPR_32bit(Section,count2,true,count2);
					}
				} else if (Section->Is32Bit(count2) && Section->MipsRegLo(count2) != RegSet->MipsRegLo(count2)) {
					Map_GPR_32bit(Section,count2,true,count2);				
				} else if (Section->Is64Bit(count2) && Section->MipsReg(count2) != RegSet->MipsReg(count2)) {
					Map_GPR_32bit(Section,count2,true,count2);
				}
			}
			Section->ResetX86Protection();
		}

		if (MemoryStackPos > 0)
		{
			Map_MemoryStack(Section,MemoryStackPos,true);
		}
	}
	Section->RegStart = Section->RegWorking;

	//Sync registers for different blocks
	char Label[100];
	sprintf(Label,"Section_%d",Section->SectionID);
	int CurrentParent = FirstParent;
	bool NeedSync = false;
	for (count = 0;count < NoOfCompiledParents;count++) {
		CRegInfo * RegSet;
		int count2;

		if (count == FirstParent) { continue; }		
		Parent    = ParentList[count].Parent;
		JumpInfo = ParentList[count].JumpInfo; 
		RegSet   = &ParentList[count].JumpInfo->RegSet;
	
		if (JumpInfo->RegSet.BlockCycleCount() != 0) { NeedSync = true; }
		if (JumpInfo->RegSet.BlockRandomModifier()  != 0) { NeedSync = true; }
		
		for (count2 = 0; count2 < 8; count2++) {
			if (Section->FpuMappedTo(count2) == (DWORD)-1) {
				NeedSync = true;
			}
		}

		for (count2 = 1; count2 < 10; count2++) {
			if (Section->x86Mapped(count2) == CRegInfo::Stack_Mapped) {
				if (Section->x86Mapped(count2) != RegSet->x86Mapped(count2)) {
					NeedSync = true;
				}
				break;
			}
		}
		for (count2 = 0; count2 < 32; count2++) {
			if (NeedSync == true)  { break; }
			if (Section->MipsRegState(count2) != RegSet->MipsRegState(count2)) {
				NeedSync = true;
				continue;
			}
			switch (Section->MipsRegState(count2)) {
			case CRegInfo::STATE_UNKNOWN: break;
			case CRegInfo::STATE_MAPPED_64:
				if (Section->MipsReg(count2) != RegSet->MipsReg(count2)) {
					NeedSync = true;
				}
				break;
			case CRegInfo::STATE_MAPPED_32_ZERO:
			case CRegInfo::STATE_MAPPED_32_SIGN:
				if (Section->MipsRegLo(count2) != RegSet->MipsRegLo(count2)) {
					//DisplayError("Parent: %d",Parent->SectionID);
					NeedSync = true;
				}
				break;
			case CRegInfo::STATE_CONST_32:
				if (Section->MipsRegLo(count2) != RegSet->MipsRegLo(count2)) {
#if (!defined(EXTERNAL_RELEASE))
					DisplayError("Umm.. how ???");
#endif
					NeedSync = true;
				}
				break;
#ifndef EXTERNAL_RELEASE
			default:
				DisplayError("Unhandled Reg state %d\nin InheritParentInfo",Section->MipsRegState(count2));
#endif
			}
		}
		if (NeedSync == false) { continue; }
		Parent   = ParentList[CurrentParent].Parent;
		JumpInfo = ParentList[CurrentParent].JumpInfo; 
		JmpLabel32(Label,0);		
		JumpInfo->LinkLocation  = RecompPos - 4;
		JumpInfo->LinkLocation2 = NULL;

		CurrentParent = count;		
		Parent   = ParentList[CurrentParent].Parent;
		JumpInfo = ParentList[CurrentParent].JumpInfo; 
		CPU_Message("   Section_%d (from %d):",Section->SectionID,Parent->SectionID);
		if (JumpInfo->LinkLocation != NULL) {
			SetJump32(JumpInfo->LinkLocation,RecompPos);
			JumpInfo->LinkLocation = NULL;
			if (JumpInfo->LinkLocation2 != NULL) { 
				SetJump32(JumpInfo->LinkLocation2,RecompPos);
				JumpInfo->LinkLocation2 = NULL;
			}
		}
		Section->RegWorking = JumpInfo->RegSet;
		if (Section->StartPC < Parent->CompilePC )
		{
			UpdateCounters(&Section->BlockCycleCount(),&Section->BlockRandomModifier(),true);
			CompileSystemCheck(Section->StartPC,Section->RegWorking);
		} else {
			UpdateCounters(&Section->BlockCycleCount(),&Section->BlockRandomModifier(),false);
		}
		SyncRegState(Section,&Section->RegStart); 		//Sync				
		Section->RegStart = Section->RegWorking;

	}

	for (count = 0;count < NoOfCompiledParents;count++) {
		Parent   = ParentList[count].Parent;
		JumpInfo = ParentList[count].JumpInfo; 

		if (JumpInfo->LinkLocation != NULL) {
			SetJump32(JumpInfo->LinkLocation,RecompPos);
			JumpInfo->LinkLocation = NULL;
			if (JumpInfo->LinkLocation2 != NULL) { 
				SetJump32(JumpInfo->LinkLocation2,RecompPos);
				JumpInfo->LinkLocation2 = NULL;
			}
		}
	}

	CPU_Message("   Section_%d:",Section->SectionID);
	Section->BlockCycleCount() = 0;
	Section->BlockRandomModifier() = 0;
	return true;
}

void LoadOpcode (DWORD Address)
{
	__try {
		if (!r4300i_LW_VAddr(Address, &g_Opcode.Hex)) {
			DisplayError(GS(MSG_FAIL_LOAD_WORD));
			ExitThread(0);
		} 
	} __except( r4300i_CPU_MemoryFilter( GetExceptionCode(), GetExceptionInformation()) ) {
		DisplayError(GS(MSG_UNKNOWN_MEM_ACTION));
		ExitThread(0);
	}
}

bool CRecompiler::GenerateX86Code(CBlockInfo & BlockInfo, CBlockSection * Section, DWORD Test )
{
	if (Section == NULL) { return false; }

	if (Section->CompiledLocation != NULL) { 		
		if (Section->Test == Test) { return false; }
		Section->Test = Test;
		if (GenerateX86Code(BlockInfo,Section->ContinueSection,Test)) { return true; }
		if (GenerateX86Code(BlockInfo,Section->JumpSection,Test)) { return true; }
		return false; 
	}
	if (Section->ParentSection.size() > 0)
	{
		for (SECTION_LIST::iterator iter = Section->ParentSection.begin(); iter != Section->ParentSection.end(); iter++)
		{
			CBlockSection * Parent = *iter;
			if (Parent->CompiledLocation != NULL) { continue; }
			if (Section->IsAllParentLoops(Parent,true,CBlockSection::GetNewTestValue())) { continue; }
			return false;
		}
	}
	if (!InheritParentInfo(Section)) { return false; }
	Section->CompiledLocation = RecompPos;
	Section->CompilePC = Section->StartPC;
	NextInstruction = NORMAL;	
	/*if (m_SyncSystem) { 
	//if (m_SyncSystem && (DWORD)RecompPos > 0x6094C283) { 
		MoveConstToVariable(Section->StartPC,&PROGRAM_COUNTER,"PROGRAM_COUNTER");	
		if (BlockCycleCount != 0) { 
			AddConstToVariable(BlockCycleCount,&CP0[9],Cop0_Name[9]); 
			SubConstFromVariable(BlockCycleCount,&Timers.Timer,"Timer");
		}
		if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1],Cop0_Name[1]); }
		BlockCycleCount = 0;
		BlockRandomModifier = 0;
		Call_Direct(SyncToPC, "SyncToPC"); 
		MoveConstToVariable((DWORD)RecompPos,&CurrentBlock,"CurrentBlock");
	}*/
	do {
		LoadOpcode(Section->CompilePC);

		//if (Section->CompilePC == 0x800AA51C && NextInstruction == NORMAL) { _asm int 3 }
//		if (Section->CompilePC == 0xF000044 && NextInstruction == NORMAL) 
//		{
//			WriteBackRegisters(Section);
//			UpdateCounters(&Section->BlockCycleCount(),&Section->BlockRandomModifier(),false);
//
//			CompConstToVariable(0x26D5BB0,&CP0[9],Cop0_Name[9]);
//			JlLabel8("blah",0);
//			BYTE * Jump = RecompPos - 1;
//		//	BreakPoint(__FILE__,__LINE__); 
//			X86BreakPoint(__FILE__,__LINE__); 
//			*((BYTE *)(Jump))=(BYTE)(RecompPos - Jump - 1);			
//		}
		
		/*if (Section->CompilePC >= 0x800C4024 && Section->CompilePC < 0x800C4030) {
			CurrentRoundingModel = RoundUnknown;
		}*/

//		if (Section->CompilePC >= 0x800017A8 && Section->CompilePC < 0x800017DC && NextInstruction == NORMAL) {
//			WriteBackRegisters(Section); 
//			UpdateCounters(&Section->BlockCycleCount(),&Section->BlockRandomModifier(),false);
//			MoveConstToVariable(Section->CompilePC,&PROGRAM_COUNTER,"PROGRAM_COUNTER");
//			//MoveConstToVariable((DWORD)RecompPos,&CurrentBlock,"CurrentBlock");
//			if (m_SyncSystem) { Call_Direct(SyncToPC, "SyncToPC"); }
//		}

//		if (Section->CompilePC >= 0x8005E984 && Section->CompilePC < 0x8005EA84 && NextInstruction == NORMAL) { 
//			WriteBackRegisters(Section);
//			UpdateCounters(&Section->BlockCycleCount(),&Section->BlockRandomModifier(),false);
//			MoveConstToVariable(Section->CompilePC,&PROGRAM_COUNTER,"PROGRAM_COUNTER");
//			//MoveConstToVariable((DWORD)RecompPos,&CurrentBlock,"CurrentBlock");
//			if (m_SyncSystem) { Call_Direct(SyncToPC, "SyncToPC"); }
//		}
		
//		if (Section->CompilePC >= 0xF000000 && Section->CompilePC < 0xF000500 && NextInstruction == NORMAL) { 
//			WriteBackRegisters(Section);
//			UpdateCounters(&Section->BlockCycleCount(),&Section->BlockRandomModifier(),false);
//			MoveConstToVariable(Section->CompilePC,&PROGRAM_COUNTER,"PROGRAM_COUNTER");
//			//MoveConstToVariable((DWORD)RecompPos,&CurrentBlock,"CurrentBlock");
//			if (m_SyncSystem) { Call_Direct(SyncToPC, "SyncToPC"); }
//		}

		/*if (Section->CompilePC == 0x802000D0 && NextInstruction == NORMAL) { 
			CPU_Message("%s = %d",GPR_Name[14],Section->MipsRegState(14));
		}*/
		/*if (Section->CompilePC == 0x150A1514 && NextInstruction == NORMAL) { 
			CPU_Message("%s = %d",GPR_Name[14],Section->MipsRegState(14));
		}
		if (Section->CompilePC == 0x150A1454 && NextInstruction == NORMAL) { 
			CPU_Message("%s = %d",GPR_Name[14],Section->MipsRegState(14));
		}*/

		if (Section->CompilePC > Section->BlockInfo->EndVAddr)
		{
			Section->BlockInfo->EndVAddr = Section->CompilePC;
		}
		Section->BlockCycleCount() += CountPerOp();
		//CPU_Message("BlockCycleCount = %d",BlockCycleCount);
		Section->BlockRandomModifier() += 1;
		//CPU_Message("BlockRandomModifier = %d",BlockRandomModifier);
				
		Section->ResetX86Protection();

		switch (g_Opcode.op) {
		case R4300i_SPECIAL:
			switch (g_Opcode.funct) {
			case R4300i_SPECIAL_SLL: Compile_R4300i_SPECIAL_SLL(Section); break;
			case R4300i_SPECIAL_SRL: Compile_R4300i_SPECIAL_SRL(Section); break;
			case R4300i_SPECIAL_SRA: Compile_R4300i_SPECIAL_SRA(Section); break;
			case R4300i_SPECIAL_SLLV: Compile_R4300i_SPECIAL_SLLV(Section); break;
			case R4300i_SPECIAL_SRLV: Compile_R4300i_SPECIAL_SRLV(Section); break;
			case R4300i_SPECIAL_SRAV: Compile_R4300i_SPECIAL_SRAV(Section); break;
			case R4300i_SPECIAL_JR: Compile_R4300i_SPECIAL_JR(Section); break;
			case R4300i_SPECIAL_JALR: Compile_R4300i_SPECIAL_JALR(Section); break;
			case R4300i_SPECIAL_MFLO: Compile_R4300i_SPECIAL_MFLO(Section); break;
			case R4300i_SPECIAL_SYSCALL: Compile_R4300i_SPECIAL_SYSCALL(Section); break;
			case R4300i_SPECIAL_MTLO: Compile_R4300i_SPECIAL_MTLO(Section); break;
			case R4300i_SPECIAL_MFHI: Compile_R4300i_SPECIAL_MFHI(Section); break;
			case R4300i_SPECIAL_MTHI: Compile_R4300i_SPECIAL_MTHI(Section); break;
			case R4300i_SPECIAL_DSLLV: Compile_R4300i_SPECIAL_DSLLV(Section); break;
			case R4300i_SPECIAL_DSRLV: Compile_R4300i_SPECIAL_DSRLV(Section); break;
			case R4300i_SPECIAL_DSRAV: Compile_R4300i_SPECIAL_DSRAV(Section); break;
			case R4300i_SPECIAL_MULT: Compile_R4300i_SPECIAL_MULT(Section); break;
			case R4300i_SPECIAL_DIV: Compile_R4300i_SPECIAL_DIV(Section); break;
			case R4300i_SPECIAL_DIVU: Compile_R4300i_SPECIAL_DIVU(Section); break;
			case R4300i_SPECIAL_MULTU: Compile_R4300i_SPECIAL_MULTU(Section); break;
			case R4300i_SPECIAL_DMULT: Compile_R4300i_SPECIAL_DMULT(Section); break;
			case R4300i_SPECIAL_DMULTU: Compile_R4300i_SPECIAL_DMULTU(Section); break;
			case R4300i_SPECIAL_DDIV: Compile_R4300i_SPECIAL_DDIV(Section); break;
			case R4300i_SPECIAL_DDIVU: Compile_R4300i_SPECIAL_DDIVU(Section); break;
			case R4300i_SPECIAL_ADD: Compile_R4300i_SPECIAL_ADD(Section); break;
			case R4300i_SPECIAL_ADDU: Compile_R4300i_SPECIAL_ADDU(Section); break;
			case R4300i_SPECIAL_SUB: Compile_R4300i_SPECIAL_SUB(Section); break;
			case R4300i_SPECIAL_SUBU: Compile_R4300i_SPECIAL_SUBU(Section); break;
			case R4300i_SPECIAL_AND: Compile_R4300i_SPECIAL_AND(Section); break;
			case R4300i_SPECIAL_OR: Compile_R4300i_SPECIAL_OR(Section); break;
			case R4300i_SPECIAL_XOR: Compile_R4300i_SPECIAL_XOR(Section); break;
			case R4300i_SPECIAL_NOR: Compile_R4300i_SPECIAL_NOR(Section); break;
			case R4300i_SPECIAL_SLT: Compile_R4300i_SPECIAL_SLT(Section); break;
			case R4300i_SPECIAL_SLTU: Compile_R4300i_SPECIAL_SLTU(Section); break;
			case R4300i_SPECIAL_DADD: Compile_R4300i_SPECIAL_DADD(Section); break;
			case R4300i_SPECIAL_DADDU: Compile_R4300i_SPECIAL_DADDU(Section); break;
			case R4300i_SPECIAL_DSUB: Compile_R4300i_SPECIAL_DSUB(Section); break;
			case R4300i_SPECIAL_DSUBU: Compile_R4300i_SPECIAL_DSUBU(Section); break;
			case R4300i_SPECIAL_DSLL: Compile_R4300i_SPECIAL_DSLL(Section); break;
			case R4300i_SPECIAL_DSRL: Compile_R4300i_SPECIAL_DSRL(Section); break;
			case R4300i_SPECIAL_DSRA: Compile_R4300i_SPECIAL_DSRA(Section); break;
			case R4300i_SPECIAL_DSLL32: Compile_R4300i_SPECIAL_DSLL32(Section); break;
			case R4300i_SPECIAL_DSRL32: Compile_R4300i_SPECIAL_DSRL32(Section); break;
			case R4300i_SPECIAL_DSRA32: Compile_R4300i_SPECIAL_DSRA32(Section); break;
			default:
				Compile_R4300i_UnknownOpcode(Section); break;
			}
			break;
		case R4300i_REGIMM: 
			switch (g_Opcode.rt) {
			case R4300i_REGIMM_BLTZ:Compile_R4300i_Branch(Section,BLTZ_Compare,BranchTypeRs, false); break;
			case R4300i_REGIMM_BGEZ:Compile_R4300i_Branch(Section,BGEZ_Compare,BranchTypeRs, false); break;
			case R4300i_REGIMM_BLTZL:Compile_R4300i_BranchLikely(Section,BLTZ_Compare, false); break;
			case R4300i_REGIMM_BGEZL:Compile_R4300i_BranchLikely(Section,BGEZ_Compare, false); break;
			case R4300i_REGIMM_BLTZAL:Compile_R4300i_Branch(Section,BLTZ_Compare,BranchTypeRs, true); break;
			case R4300i_REGIMM_BGEZAL:Compile_R4300i_Branch(Section,BGEZ_Compare,BranchTypeRs, true); break;
			default:
				Compile_R4300i_UnknownOpcode(Section); break;
			}
			break;
		case R4300i_BEQ: Compile_R4300i_Branch(Section,BEQ_Compare,BranchTypeRsRt,false); break;
		case R4300i_BNE: Compile_R4300i_Branch(Section,BNE_Compare,BranchTypeRsRt,false); break;
		case R4300i_BGTZ:Compile_R4300i_Branch(Section,BGTZ_Compare,BranchTypeRs,false); break;
		case R4300i_BLEZ:Compile_R4300i_Branch(Section,BLEZ_Compare,BranchTypeRs,false); break;
		case R4300i_J: Compile_R4300i_J(Section); break;
		case R4300i_JAL: Compile_R4300i_JAL(Section); break;
		case R4300i_ADDI: Compile_R4300i_ADDI(Section); break;
		case R4300i_ADDIU: Compile_R4300i_ADDIU(Section); break;
		case R4300i_SLTI: Compile_R4300i_SLTI(Section); break;
		case R4300i_SLTIU: Compile_R4300i_SLTIU(Section); break;
		case R4300i_ANDI: Compile_R4300i_ANDI(Section); break;
		case R4300i_ORI: Compile_R4300i_ORI(Section); break;
		case R4300i_XORI: Compile_R4300i_XORI(Section); break;
		case R4300i_LUI: Compile_R4300i_LUI(Section); break;
		case R4300i_CP0:
			switch (g_Opcode.rs) {
			case R4300i_COP0_MF: Compile_R4300i_COP0_MF(Section); break;
			case R4300i_COP0_MT: Compile_R4300i_COP0_MT(Section); break;
			default:
				if ( (g_Opcode.rs & 0x10 ) != 0 ) {
					switch( g_Opcode.funct ) {
					case R4300i_COP0_CO_TLBR: Compile_R4300i_COP0_CO_TLBR(Section); break;
					case R4300i_COP0_CO_TLBWI: Compile_R4300i_COP0_CO_TLBWI(Section); break;
					case R4300i_COP0_CO_TLBWR: Compile_R4300i_COP0_CO_TLBWR(Section); break;
					case R4300i_COP0_CO_TLBP: Compile_R4300i_COP0_CO_TLBP(Section); break;
					case R4300i_COP0_CO_ERET: Compile_R4300i_COP0_CO_ERET(Section); break;
					default: Compile_R4300i_UnknownOpcode(Section); break;
					}
				} else {
					Compile_R4300i_UnknownOpcode(Section);
				}
			}
			break;
		case R4300i_CP1:
			switch (g_Opcode.rs) {
			case R4300i_COP1_MF: Compile_R4300i_COP1_MF(Section); break;
			case R4300i_COP1_DMF: Compile_R4300i_COP1_DMF(Section); break;
			case R4300i_COP1_CF: Compile_R4300i_COP1_CF(Section); break;
			case R4300i_COP1_MT: Compile_R4300i_COP1_MT(Section); break;
			case R4300i_COP1_DMT: Compile_R4300i_COP1_DMT(Section); break;
			case R4300i_COP1_CT: Compile_R4300i_COP1_CT(Section); break;
			case R4300i_COP1_BC:
				switch (g_Opcode.ft) {
				case R4300i_COP1_BC_BCF: Compile_R4300i_Branch(Section,COP1_BCF_Compare,BranchTypeCop1,false); break;
				case R4300i_COP1_BC_BCT: Compile_R4300i_Branch(Section,COP1_BCT_Compare,BranchTypeCop1,false); break;
				case R4300i_COP1_BC_BCFL: Compile_R4300i_BranchLikely(Section,COP1_BCF_Compare,false); break;
				case R4300i_COP1_BC_BCTL: Compile_R4300i_BranchLikely(Section,COP1_BCT_Compare,false); break;
				default:
					Compile_R4300i_UnknownOpcode(Section); break;
				}
				break;
			case R4300i_COP1_S: 
				switch (g_Opcode.funct) {
				case R4300i_COP1_FUNCT_ADD: Compile_R4300i_COP1_S_ADD(Section); break;
				case R4300i_COP1_FUNCT_SUB: Compile_R4300i_COP1_S_SUB(Section); break;
				case R4300i_COP1_FUNCT_MUL: Compile_R4300i_COP1_S_MUL(Section); break;
				case R4300i_COP1_FUNCT_DIV: Compile_R4300i_COP1_S_DIV(Section); break;
				case R4300i_COP1_FUNCT_ABS: Compile_R4300i_COP1_S_ABS(Section); break;
				case R4300i_COP1_FUNCT_NEG: Compile_R4300i_COP1_S_NEG(Section); break;
				case R4300i_COP1_FUNCT_SQRT: Compile_R4300i_COP1_S_SQRT(Section); break;
				case R4300i_COP1_FUNCT_MOV: Compile_R4300i_COP1_S_MOV(Section); break;
				case R4300i_COP1_FUNCT_TRUNC_L: Compile_R4300i_COP1_S_TRUNC_L(Section); break;
				case R4300i_COP1_FUNCT_CEIL_L: Compile_R4300i_COP1_S_CEIL_L(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_FLOOR_L: Compile_R4300i_COP1_S_FLOOR_L(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_ROUND_W: Compile_R4300i_COP1_S_ROUND_W(Section); break;
				case R4300i_COP1_FUNCT_TRUNC_W: Compile_R4300i_COP1_S_TRUNC_W(Section); break;
				case R4300i_COP1_FUNCT_CEIL_W: Compile_R4300i_COP1_S_CEIL_W(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_FLOOR_W: Compile_R4300i_COP1_S_FLOOR_W(Section); break;
				case R4300i_COP1_FUNCT_CVT_D: Compile_R4300i_COP1_S_CVT_D(Section); break;
				case R4300i_COP1_FUNCT_CVT_W: Compile_R4300i_COP1_S_CVT_W(Section); break;
				case R4300i_COP1_FUNCT_CVT_L: Compile_R4300i_COP1_S_CVT_L(Section); break;
				case R4300i_COP1_FUNCT_C_F:   case R4300i_COP1_FUNCT_C_UN:
				case R4300i_COP1_FUNCT_C_EQ:  case R4300i_COP1_FUNCT_C_UEQ:
				case R4300i_COP1_FUNCT_C_OLT: case R4300i_COP1_FUNCT_C_ULT:
				case R4300i_COP1_FUNCT_C_OLE: case R4300i_COP1_FUNCT_C_ULE:
				case R4300i_COP1_FUNCT_C_SF:  case R4300i_COP1_FUNCT_C_NGLE:
				case R4300i_COP1_FUNCT_C_SEQ: case R4300i_COP1_FUNCT_C_NGL:
				case R4300i_COP1_FUNCT_C_LT:  case R4300i_COP1_FUNCT_C_NGE:
				case R4300i_COP1_FUNCT_C_LE:  case R4300i_COP1_FUNCT_C_NGT:
					Compile_R4300i_COP1_S_CMP(Section); break;
				default:
					Compile_R4300i_UnknownOpcode(Section); break;
				}
				break;
			case R4300i_COP1_D: 
				switch (g_Opcode.funct) {
				case R4300i_COP1_FUNCT_ADD: Compile_R4300i_COP1_D_ADD(Section); break;
				case R4300i_COP1_FUNCT_SUB: Compile_R4300i_COP1_D_SUB(Section); break;
				case R4300i_COP1_FUNCT_MUL: Compile_R4300i_COP1_D_MUL(Section); break;
				case R4300i_COP1_FUNCT_DIV: Compile_R4300i_COP1_D_DIV(Section); break;
				case R4300i_COP1_FUNCT_ABS: Compile_R4300i_COP1_D_ABS(Section); break;
				case R4300i_COP1_FUNCT_NEG: Compile_R4300i_COP1_D_NEG(Section); break;
				case R4300i_COP1_FUNCT_SQRT: Compile_R4300i_COP1_D_SQRT(Section); break;
				case R4300i_COP1_FUNCT_MOV: Compile_R4300i_COP1_D_MOV(Section); break;
				case R4300i_COP1_FUNCT_TRUNC_L: Compile_R4300i_COP1_D_TRUNC_L(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_CEIL_L: Compile_R4300i_COP1_D_CEIL_L(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_FLOOR_L: Compile_R4300i_COP1_D_FLOOR_L(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_ROUND_W: Compile_R4300i_COP1_D_ROUND_W(Section); break;
				case R4300i_COP1_FUNCT_TRUNC_W: Compile_R4300i_COP1_D_TRUNC_W(Section); break;
				case R4300i_COP1_FUNCT_CEIL_W: Compile_R4300i_COP1_D_CEIL_W(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_FLOOR_W: Compile_R4300i_COP1_D_FLOOR_W(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_CVT_S: Compile_R4300i_COP1_D_CVT_S(Section); break;
				case R4300i_COP1_FUNCT_CVT_W: Compile_R4300i_COP1_D_CVT_W(Section); break;
				case R4300i_COP1_FUNCT_CVT_L: Compile_R4300i_COP1_D_CVT_L(Section); break;
				case R4300i_COP1_FUNCT_C_F:   case R4300i_COP1_FUNCT_C_UN:
				case R4300i_COP1_FUNCT_C_EQ:  case R4300i_COP1_FUNCT_C_UEQ:
				case R4300i_COP1_FUNCT_C_OLT: case R4300i_COP1_FUNCT_C_ULT:
				case R4300i_COP1_FUNCT_C_OLE: case R4300i_COP1_FUNCT_C_ULE:
				case R4300i_COP1_FUNCT_C_SF:  case R4300i_COP1_FUNCT_C_NGLE:
				case R4300i_COP1_FUNCT_C_SEQ: case R4300i_COP1_FUNCT_C_NGL:
				case R4300i_COP1_FUNCT_C_LT:  case R4300i_COP1_FUNCT_C_NGE:
				case R4300i_COP1_FUNCT_C_LE:  case R4300i_COP1_FUNCT_C_NGT:
					Compile_R4300i_COP1_D_CMP(Section); break;
				default:
					Compile_R4300i_UnknownOpcode(Section); break;
				}
				break;
			case R4300i_COP1_W: 
				switch (g_Opcode.funct) {
				case R4300i_COP1_FUNCT_CVT_S: Compile_R4300i_COP1_W_CVT_S(Section); break;
				case R4300i_COP1_FUNCT_CVT_D: Compile_R4300i_COP1_W_CVT_D(Section); break;
				default:
					Compile_R4300i_UnknownOpcode(Section); break;
				}
				break;
			case R4300i_COP1_L: 
				switch (g_Opcode.funct) {
				case R4300i_COP1_FUNCT_CVT_S: Compile_R4300i_COP1_L_CVT_S(Section); break;
				case R4300i_COP1_FUNCT_CVT_D: Compile_R4300i_COP1_L_CVT_D(Section); break;
				default:
					Compile_R4300i_UnknownOpcode(Section); break;
				}
				break;
			default:
				Compile_R4300i_UnknownOpcode(Section); break;
			}
			break;
		case R4300i_BEQL: Compile_R4300i_BranchLikely(Section,BEQ_Compare,false); break;
		case R4300i_BNEL: Compile_R4300i_BranchLikely(Section,BNE_Compare,false); break;
		case R4300i_BGTZL:Compile_R4300i_BranchLikely(Section,BGTZ_Compare,false); break;
		case R4300i_BLEZL:Compile_R4300i_BranchLikely(Section,BLEZ_Compare,false); break;
		case R4300i_DADDIU: Compile_R4300i_DADDIU(Section); break;
		case R4300i_LDL: Compile_R4300i_LDL(Section); break;
		case R4300i_LDR: Compile_R4300i_LDR(Section); break;
		case R4300i_LB: Compile_R4300i_LB(Section); break;
		case R4300i_LH: Compile_R4300i_LH(Section); break;
		case R4300i_LWL: Compile_R4300i_LWL(Section); break;
		case R4300i_LW: Compile_R4300i_LW(Section); break;
		case R4300i_LBU: Compile_R4300i_LBU(Section); break;
		case R4300i_LHU: Compile_R4300i_LHU(Section); break;
		case R4300i_LWR: Compile_R4300i_LWR(Section); break;
		case R4300i_LWU: Compile_R4300i_LWU(Section); break;	//added by Witten
		case R4300i_SB: Compile_R4300i_SB(Section); break;
		case R4300i_SH: Compile_R4300i_SH(Section); break;
		case R4300i_SWL: Compile_R4300i_SWL(Section); break;
		case R4300i_SW: Compile_R4300i_SW(Section); break;
		case R4300i_SWR: Compile_R4300i_SWR(Section); break;
		case R4300i_SDL: Compile_R4300i_SDL(Section); break;
		case R4300i_SDR: Compile_R4300i_SDR(Section); break;
		case R4300i_CACHE: Compile_R4300i_CACHE(Section); break;
		case R4300i_LL: Compile_R4300i_LL(Section); break;
		case R4300i_LWC1: Compile_R4300i_LWC1(Section); break;
		case R4300i_LDC1: Compile_R4300i_LDC1(Section); break;
		case R4300i_SC: Compile_R4300i_SC(Section); break;
		case R4300i_LD: Compile_R4300i_LD(Section); break;
		case R4300i_SWC1: Compile_R4300i_SWC1(Section); break;
		case R4300i_SDC1: Compile_R4300i_SDC1(Section); break;
		case R4300i_SD: Compile_R4300i_SD(Section); break;
		default:
			Compile_R4300i_UnknownOpcode(Section); break;
		}

		if (!bRegCaching()) { WriteBackRegisters(Section); }
		Section->ResetX86Protection();
		
		/*if ((DWORD)RecompPos > 0x60B452E6) {
			if (Section->CompilePC == 0x8002D9B8 && Section->CompilePC < 0x8002DA20) {
				CurrentRoundingModel = RoundUnknown;
			}
		}*/
		UnMap_AllFPRs(Section);
		
		/*if ((DWORD)RecompPos > 0x60AD0BD3) {
			if (Section->CompilePC >= 0x8008B804 && Section->CompilePC < 0x800496D8) {
				CPU_Message("Blah *");
				WriteBackRegisters(Section);
			}
			/*if (Section->CompilePC >= 0x80000180 && Section->CompilePC < 0x80000190) {
				CPU_Message("Blah *");
				//WriteBackRegisters(Section);
			}*/
		//}

		/*for (count = 1; count < 10; count ++) { 
			if (Section->x86Mapped(count) == CRegInfo::Stack_Mapped) { 
				UnMap_X86reg (Section, count); 
			}
		}*/
		//CPU_Message("MemoryStack = %s",Map_MemoryStack(Section, false) > 0?x86_Name(Map_MemoryStack(Section, false)):"Not Mapped");

		if ((Section->CompilePC &0xFFC) == 0xFFC) {
			if (NextInstruction == DO_DELAY_SLOT) {
#ifndef EXTERNAL_RELEASE
				DisplayError("Wanting to do delay slot over end of block");
#endif
			}
			if (NextInstruction == NORMAL) {
				CompileExit (Section,Section->CompilePC, Section->CompilePC + 4,Section->RegWorking,CExitInfo::Normal,true,NULL);
				NextInstruction = END_BLOCK;
			}
		}

		if (Section->DelaySlotSection)
		{
			Section->Cont.RegSet = Section->RegWorking;
			GenerateSectionLinkage(Section);			
			NextInstruction = END_BLOCK;
		}

		switch (NextInstruction) {
		case NORMAL: 
			Section->CompilePC += 4; 
			break;
		case DO_DELAY_SLOT:
			NextInstruction = DELAY_SLOT;
			Section->CompilePC += 4; 
			break;
		case DELAY_SLOT:
			NextInstruction = DELAY_SLOT_DONE;
			Section->BlockCycleCount() -= CountPerOp();
			Section->BlockRandomModifier() -= 1;
			Section->CompilePC -= 4; 
			break;
		}
	} while (NextInstruction != END_BLOCK);
	
	return true;
}


void CRecompiler::CompileExit ( CBlockSection * Section, DWORD JumpPC, DWORD TargetPC, CRegInfo ExitRegSet, CExitInfo::EXIT_REASON reason, int CompileNow, void (*x86Jmp)(char * Label, DWORD Value))
{
	CBlockInfo * const BlockInfo = Section->BlockInfo;

	if (!CompileNow) 
	{
		char String[100];
		sprintf(String,"Exit_%d",BlockInfo->ExitInfo.size());
		if (x86Jmp == NULL) 
		{ 
			DisplayError("CompileExit error");
			ExitThread(0);
		}
		x86Jmp(String,0);

		CExitInfo ExitInfo;
		ExitInfo.ID = BlockInfo->ExitInfo.size();
		ExitInfo.TargetPC = TargetPC;
		ExitInfo.ExitRegSet = ExitRegSet;
		ExitInfo.reason = reason;
		ExitInfo.NextInstruction = NextInstruction;
		ExitInfo.JumpLoc = RecompPos - 4;
		BlockInfo->ExitInfo.push_back(ExitInfo);
		return;
	}
	
	//CPU_Message("CompileExit: %d",reason);
	CBlockSection TempSection(BlockInfo,-1,0);
	TempSection.AddParent(NULL);
	TempSection.RegWorking = ExitRegSet;

	if (TargetPC != (DWORD)-1) 
	{
		MoveConstToVariable(TargetPC,&PROGRAM_COUNTER,"PROGRAM_COUNTER"); 
		UpdateCounters(&ExitRegSet.BlockCycleCount(),&ExitRegSet.BlockRandomModifier(),TargetPC <= JumpPC && reason == CExitInfo::Normal);
	} else {
		UpdateCounters(&ExitRegSet.BlockCycleCount(),&ExitRegSet.BlockRandomModifier(),reason == CExitInfo::Normal);
	}
	WriteBackRegisters(&TempSection);

	switch (reason) {
	case CExitInfo::Normal: case CExitInfo::Normal_NoSysCheck:
		TempSection.RegWorking.BlockRandomModifier() = 0;
		TempSection.RegWorking.BlockCycleCount() = 0;
		if (TargetPC != (DWORD)-1)
		{
			if (TargetPC <= JumpPC && reason == CExitInfo::Normal)
			{
				CompileSystemCheck((DWORD)-1,TempSection.RegWorking);
			}
		} else {
			if (reason == CExitInfo::Normal) { CompileSystemCheck((DWORD)-1,TempSection.RegWorking);	}
		}
		if (m_SyncSystem) { Call_Direct(SyncToPC, "SyncToPC"); }
#ifdef LinkBlocks
		if (bSMM_ValidFunc == false)
		{
			if (LookUpMode() == FuncFind_ChangeMemory) 
			{
				BreakPoint(__FILE__,__LINE__);
	//			BYTE * Jump, * Jump2;
	//			if (TargetPC >= 0x80000000 && TargetPC < 0xC0000000) {
	//				DWORD pAddr = TargetPC & 0x1FFFFFFF;
	//	
	//				MoveVariableToX86reg((BYTE *)N64MEM + pAddr,"N64MEM + pAddr",x86_EAX);
	//				Jump2 = NULL;
	//			} else {				
	//				MoveConstToX86reg((TargetPC >> 12),x86_ECX);
	//				MoveConstToX86reg(TargetPC,x86_EBX);
	//				MoveVariableDispToX86Reg(TLB_ReadMap,"TLB_ReadMap",x86_ECX,x86_ECX,4);
	//				TestX86RegToX86Reg(x86_ECX,x86_ECX);
	//				JeLabel8("NoTlbEntry",0);
	//				Jump2 = RecompPos - 1;
	//				MoveX86regPointerToX86reg(x86_ECX, x86_EBX,x86_EAX);
	//			}
	//			MoveX86RegToX86Reg(x86_EAX,x86_ECX);
	//			AndConstToX86Reg(x86_ECX,0xFFFF0000);
	//			CompConstToX86reg(x86_ECX,0x7C7C0000);
	//			JneLabel8("NoCode",0);
	//			Jump = RecompPos - 1;
	//			AndConstToX86Reg(x86_EAX,0xFFFF);
	//			ShiftLeftSignImmed(x86_EAX,4);
	//			AddConstToX86Reg(x86_EAX,0xC);
	//			MoveVariableDispToX86Reg(OrigMem,"OrigMem",x86_ECX,x86_EAX,1);
	//			JmpDirectReg(x86_ECX);
	//			CPU_Message("      NoCode:");
	//			*((BYTE *)(Jump))=(BYTE)(RecompPos - Jump - 1);
	//			if (Jump2 != NULL) {
	//				CPU_Message("      NoTlbEntry:");
	//				*((BYTE *)(Jump2))=(BYTE)(RecompPos - Jump2 - 1);
	//			}
			} 
			else if (LookUpMode() == FuncFind_VirtualLookup)
			{			
				MoveConstToX86reg(TargetPC,x86_EDX);
				MoveConstToX86reg((DWORD)&m_Functions,x86_ECX);		
				Call_Direct(AddressOf(CFunctionMap::CompilerFindFunction), "CFunctionMap::CompilerFindFunction");
				MoveX86RegToX86Reg(x86_EAX,x86_ECX);
				JecxzLabel8("NullPointer",0);
				BYTE * Jump = RecompPos - 1;
				MoveX86PointerToX86regDisp(x86_EBX,x86_ECX,0xC);				
				JmpDirectReg(x86_EBX);
				CPU_Message("      NullPointer:");
				*((BYTE *)(Jump))=(BYTE)(RecompPos - Jump - 1);
			}
			else if (LookUpMode() == FuncFind_PhysicalLookup) 
			{
				BYTE * Jump2 = NULL;
				if (TargetPC >= 0x80000000 && TargetPC < 0x90000000) {
					DWORD pAddr = TargetPC & 0x1FFFFFFF;
					MoveVariableToX86reg((BYTE *)JumpTable + pAddr,"JumpTable + pAddr",x86_ECX);
				} else if (TargetPC >= 0x90000000 && TargetPC < 0xC0000000) {
				} else {				
					MoveConstToX86reg((TargetPC >> 12),x86_ECX);
					MoveConstToX86reg(TargetPC,x86_EBX);
					MoveVariableDispToX86Reg(TLB_ReadMap,"TLB_ReadMap",x86_ECX,x86_ECX,4);
					TestX86RegToX86Reg(x86_ECX,x86_ECX);
					JeLabel8("NoTlbEntry",0);
					Jump2 = RecompPos - 1;
					AddConstToX86Reg(x86_ECX,(DWORD)JumpTable - (DWORD)N64MEM);
					MoveX86regPointerToX86reg(x86_ECX, x86_EBX,x86_ECX);
				}
				if (TargetPC < 0x90000000 || TargetPC >= 0xC0000000)
				{
					JecxzLabel8("NullPointer",0);
					BYTE * Jump = RecompPos - 1;
					MoveX86PointerToX86regDisp(x86_EAX,x86_ECX,0xC);				
					JmpDirectReg(x86_EAX);
					CPU_Message("      NullPointer:");
					*((BYTE *)(Jump))=(BYTE)(RecompPos - Jump - 1);
					if (Jump2 != NULL) {
						CPU_Message("      NoTlbEntry:");
						*((BYTE *)(Jump2))=(BYTE)(RecompPos - Jump2 - 1);
					}
				}
			}
		}
		Ret();
#else
		Ret();
#endif
		break;
	case CExitInfo::DoCPU_Action:
		Call_Direct(DoSomething,"DoSomething");
		if (m_SyncSystem) { Call_Direct(SyncToPC, "SyncToPC"); }
		Ret();
		break;
	case CExitInfo::DoSysCall:
		MoveConstToX86reg(NextInstruction == JUMP || NextInstruction == DELAY_SLOT,x86_ECX);		
		Call_Direct(DoSysCallException,"DoSysCallException");
		if (m_SyncSystem) { Call_Direct(SyncToPC, "SyncToPC"); }
		Ret();
		break;
	case CExitInfo::COP1_Unuseable:
		MoveConstToX86reg(NextInstruction == JUMP || NextInstruction == DELAY_SLOT,x86_ECX);		
		MoveConstToX86reg(1,x86_EDX);
		Call_Direct(DoCopUnusableException,"DoCopUnusableException");
		if (m_SyncSystem) { Call_Direct(SyncToPC, "SyncToPC"); }
		Ret();
		break;
	case CExitInfo::ExitResetRecompCode:
		if (NextInstruction == JUMP || NextInstruction == DELAY_SLOT) {
			X86BreakPoint(__FILE__,__LINE__);
		}
		if (m_SyncSystem) { Call_Direct(SyncToPC, "SyncToPC"); }
		X86BreakPoint(__FILE__,__LINE__);
		MoveVariableToX86reg(this,"this",x86_ECX);		
		Call_Direct(AddressOf(&CRecompiler::ResetRecompCode), "ResetRecompCode");
		Ret();
		break;
	case CExitInfo::TLBReadMiss:
		MoveConstToX86reg(NextInstruction == JUMP || NextInstruction == DELAY_SLOT,x86_ECX);
		MoveVariableToX86reg(&TLBLoadAddress,"TLBLoadAddress",x86_EDX);
		Call_Direct(DoTLBMiss,"DoTLBMiss");
		if (m_SyncSystem) { Call_Direct(SyncToPC, "SyncToPC"); }
		Ret();
		break;
	default:
		DisplayError("how did you want to exit on reason (%d) ???",reason);
	}
}

void CRecompiler::UpdateCounters ( DWORD * Cycles, DWORD * RandomMod, BOOL CheckTimer)
{
	if (Cycles == NULL || RandomMod == NULL)
	{
		BreakPoint(__FILE__,__LINE__); 
	}
	if (*RandomMod != 0 || *Cycles != 0) {
		WriteX86Comment("Update Counters");
	}
	if (*RandomMod != 0) { SubConstFromVariable(*RandomMod,&CP0[1],Cop0_Name[1]); }
	if (*Cycles != 0) { 
		if (m_SyncSystem) {
			char text[100];

			WriteX86Comment("Sync CPU after update of cycles");
			Pushad();
			sprintf(text,"%d",(DWORD)*Cycles);
			PushImm32(text,(DWORD)*Cycles);
			Call_Direct(UpdateSyncCPU,"UpdateSyncCPU");
			Popad();
		}
		AddConstToVariable(*Cycles,&CP0[9],Cop0_Name[9]); 
		SubConstFromVariable(*Cycles,g_Timer,"Timer");
	}
	*Cycles = 0;
	*RandomMod = 0;

	if (CheckTimer)
	{
		BYTE * Jump;

		// Timer
		if (*Cycles == 0) {
			CompConstToVariable(0,g_Timer,"Timer");
		//} else{
			//	uses SubConstFromVariable(Cycles,g_Timer,"Timer"); for compare flag
		}
		JnsLabel8("Continue_From_Timer_Test",0);
		Jump = RecompPos - 1;
		Pushad();
		Call_Direct(TimerDone,"TimerDone");
		Popad();
	
		CPU_Message("");
		CPU_Message("      $Continue_From_Timer_Test:");
		SetJump8(Jump,RecompPos);
	}
/*	if (CheckTimer)

	{
		x86ops.CompConstToVariable(0,(DWORD *)&_Reg->Timer,"_Reg->Timer");
		x86ops.JgLabel8("Timer_Fine",0); BYTE * Jump = x86ops.RecompPos.WritePos() - 1;
		Section->SaveX86Registers();
		x86ops.Call_NonStatic_Direct((CSystemTimer *)_Reg,AddressOf(CSystemTimer::CheckTimer),"CSystemTimer::CheckTimer");
		Section->RestoreX86Registers();
		x86ops.WriteLabel("Timer_Fine"); x86ops.SetJump8(Jump, x86ops.RecompPos.WritePos());
	}*/
}

void CRecompiler::CompileSystemCheck (DWORD TargetPC, CRegInfo &  RegSet)
{
	CompConstToVariable(0,&CPU_Action.DoSomething,"CPU_Action.DoSomething");
	JeLabel32("Continue_From_Interrupt_Test",0);
	BYTE * Jump = RecompPos - 4;
	if (TargetPC != (DWORD)-1) 
	{
		MoveConstToVariable(TargetPC,&PROGRAM_COUNTER,"PROGRAM_COUNTER"); 
	}

	CBlockSection Section(NULL,-1,0);
	Section.AddParent(NULL);
	Section.RegWorking = RegSet;
	WriteBackRegisters(&Section);		
	CompileExit(&Section,-1, -1,Section.RegWorking,CExitInfo::DoCPU_Action,true,NULL);
	CPU_Message("");
	CPU_Message("      $Continue_From_Interrupt_Test:");
	SetJump32(Jump,RecompPos);	
}

void SyncRegState (CBlockSection * Section, CRegInfo * SyncTo) {
	int count, x86Reg,x86RegHi, changed;
	
	changed = false;
	UnMap_AllFPRs(Section);
	if (Section->CurrentRoundingModel() != SyncTo->CurrentRoundingModel()) { Section->CurrentRoundingModel() = CRegInfo::RoundUnknown; }
	x86Reg = Map_MemoryStack(Section, x86_Any, false);
	//CPU_Message("MemoryStack for Original State = %s",x86Reg > 0?x86_Name(x86Reg):"Not Mapped");

	for (x86Reg = 1; x86Reg < 10; x86Reg ++) {
		if (Section->x86Mapped(x86Reg) != CRegInfo::Stack_Mapped) { continue; }
		if (SyncTo->x86Mapped(x86Reg) != CRegInfo::Stack_Mapped) {
			UnMap_X86reg(Section,x86Reg);
			for (count = 1; count < 10; count ++) {
				if (SyncTo->x86Mapped(count) == CRegInfo::Stack_Mapped) {
					MoveX86RegToX86Reg(count,x86Reg); 
					changed = true;
				}
			}
			if (!changed) {
				MoveVariableToX86reg(g_MemoryStack,"MemoryStack",x86Reg);
			}
			changed = true;
		}
	}
	for (x86Reg = 1; x86Reg < 10; x86Reg ++) {
		if (SyncTo->x86Mapped(x86Reg) != CRegInfo::Stack_Mapped) { continue; }
		//CPU_Message("MemoryStack for Sync State = %s",x86Reg > 0?x86_Name(x86Reg):"Not Mapped");
		if (Section->x86Mapped(x86Reg) == CRegInfo::Stack_Mapped) { break; }
		UnMap_X86reg(Section,x86Reg);		
	}
	
	for (count = 1; count < 32; count ++) {
		if (Section->MipsRegState(count) == SyncTo->MipsRegState(count)) {
			switch (Section->MipsRegState(count)) {
			case CRegInfo::STATE_UNKNOWN: continue;
			case CRegInfo::STATE_MAPPED_64:
				if (Section->MipsReg(count) == SyncTo->MipsReg(count)) {
					continue;
				}
				break;
			case CRegInfo::STATE_MAPPED_32_ZERO:
			case CRegInfo::STATE_MAPPED_32_SIGN:
				if (Section->MipsRegLo(count) == SyncTo->MipsRegLo(count)) {
					continue;
				}
				break;
			case CRegInfo::STATE_CONST_64:
				if (Section->MipsReg(count) != SyncTo->MipsReg(count)) {
#if (!defined(EXTERNAL_RELEASE))
					DisplayError("Umm.. how ???");
#endif
				}
				continue;
			case CRegInfo::STATE_CONST_32:
				if (Section->MipsRegLo(count) != SyncTo->MipsRegLo(count)) {
#if (!defined(EXTERNAL_RELEASE))
					DisplayError("Umm.. how ???");
#endif
				}
				continue;
#ifndef EXTERNAL_RELEASE
			default:
				DisplayError("Unhandled Reg state %d\nin SyncRegState",Section->MipsRegState(count));
#endif
			}			
		}
		changed = true;

		switch (SyncTo->MipsRegState(count)) {
		case CRegInfo::STATE_UNKNOWN: UnMap_GPR(Section,count,true);  break;
		case CRegInfo::STATE_MAPPED_64:
			x86Reg = SyncTo->MipsRegLo(count);
			x86RegHi = SyncTo->MipsRegHi(count);
			UnMap_X86reg(Section,x86Reg);
			UnMap_X86reg(Section,x86RegHi);
			switch (Section->MipsRegState(count)) {
			case CRegInfo::STATE_UNKNOWN:
				MoveVariableToX86reg(&GPR[count].UW[0],GPR_NameLo[count],x86Reg);
				MoveVariableToX86reg(&GPR[count].UW[1],GPR_NameHi[count],x86RegHi);
				break;
			case CRegInfo::STATE_MAPPED_64:
				MoveX86RegToX86Reg(Section->MipsRegLo(count),x86Reg); 
				Section->x86Mapped(Section->MipsRegLo(count)) = CRegInfo::NotMapped;
				MoveX86RegToX86Reg(Section->MipsRegHi(count),x86RegHi); 
				Section->x86Mapped(Section->MipsRegHi(count)) = CRegInfo::NotMapped;
				break;
			case CRegInfo::STATE_MAPPED_32_SIGN:
				MoveX86RegToX86Reg(Section->MipsRegLo(count),x86RegHi); 
				ShiftRightSignImmed(x86RegHi,31);
				MoveX86RegToX86Reg(Section->MipsRegLo(count),x86Reg); 
				Section->x86Mapped(Section->MipsRegLo(count)) = CRegInfo::NotMapped;
				break;
			case CRegInfo::STATE_MAPPED_32_ZERO:
				XorX86RegToX86Reg(x86RegHi,x86RegHi);
				MoveX86RegToX86Reg(Section->MipsRegLo(count),x86Reg); 
				Section->x86Mapped(Section->MipsRegLo(count)) = CRegInfo::NotMapped;
				break;
			case CRegInfo::STATE_CONST_64:
				MoveConstToX86reg(Section->MipsRegHi(count),x86RegHi); 
				MoveConstToX86reg(Section->MipsRegLo(count),x86Reg); 
				break;
			case CRegInfo::STATE_CONST_32:
				MoveConstToX86reg(Section->MipsRegLo_S(count) >> 31,x86RegHi); 
				MoveConstToX86reg(Section->MipsRegLo(count),x86Reg); 
				break;
			default:
#ifndef EXTERNAL_RELEASE
				CPU_Message("Do something with states in SyncRegState\nSTATE_MAPPED_64\n%d",Section->MipsRegState(count));
				DisplayError("Do something with states in SyncRegState\nSTATE_MAPPED_64\n%d",Section->MipsRegState(count));
#endif
				continue;
			}
			Section->MipsRegLo(count) = x86Reg;
			Section->MipsRegHi(count) = x86RegHi;
			Section->MipsRegState(count) = CRegInfo::STATE_MAPPED_64;
			Section->x86Mapped(x86Reg) = CRegInfo::GPR_Mapped;
			Section->x86Mapped(x86RegHi) = CRegInfo::GPR_Mapped;
			Section->x86MapOrder(x86Reg) = 1;
			Section->x86MapOrder(x86RegHi) = 1;
			break;
		case CRegInfo::STATE_MAPPED_32_SIGN:
			x86Reg = SyncTo->MipsRegLo(count);
			UnMap_X86reg(Section,x86Reg);
			switch (Section->MipsRegState(count)) {
			case CRegInfo::STATE_UNKNOWN: MoveVariableToX86reg(&GPR[count].UW[0],GPR_NameLo[count],x86Reg); break;
			case CRegInfo::STATE_CONST_32: MoveConstToX86reg(Section->MipsRegLo(count),x86Reg); break;
			case CRegInfo::STATE_MAPPED_32_SIGN: 
				MoveX86RegToX86Reg(Section->MipsRegLo(count),x86Reg); 
				Section->x86Mapped(Section->MipsRegLo(count)) = CRegInfo::NotMapped;
				break;
			case CRegInfo::STATE_MAPPED_32_ZERO:
				if (Section->MipsRegLo(count) != (DWORD)x86Reg) {
					MoveX86RegToX86Reg(Section->MipsRegLo(count),x86Reg); 
					Section->x86Mapped(Section->MipsRegLo(count)) = CRegInfo::NotMapped;
				}
				break;
			case CRegInfo::STATE_MAPPED_64:
				MoveX86RegToX86Reg(Section->MipsRegLo(count),x86Reg); 
				Section->x86Mapped(Section->MipsRegLo(count)) = CRegInfo::NotMapped;
				Section->x86Mapped(Section->MipsRegHi(count)) = CRegInfo::NotMapped;
				break;
#ifndef EXTERNAL_RELEASE
			case CRegInfo::STATE_CONST_64:
				DisplayError("hi %X\nLo %X",Section->MipsRegHi(count),Section->MipsRegLo(count));
			default:				
				CPU_Message("Do something with states in SyncRegState\nSTATE_MAPPED_32_SIGN\n%d",Section->MipsRegState(count));
				DisplayError("Do something with states in SyncRegState\nSTATE_MAPPED_32_SIGN\n%d",Section->MipsRegState(count));
#endif
			}
			Section->MipsRegLo(count) = x86Reg;
			Section->MipsRegState(count) = CRegInfo::STATE_MAPPED_32_SIGN;
			Section->x86Mapped(x86Reg) = CRegInfo::GPR_Mapped;
			Section->x86MapOrder(x86Reg) = 1;
			break;
		case CRegInfo::STATE_MAPPED_32_ZERO:
			x86Reg = SyncTo->MipsRegLo(count);
			UnMap_X86reg(Section,x86Reg);
			switch (Section->MipsRegState(count)) {
			case CRegInfo::STATE_MAPPED_64:
			case CRegInfo::STATE_UNKNOWN:  
				MoveVariableToX86reg(&GPR[count].UW[0],GPR_NameLo[count],x86Reg); 
				break;
			case CRegInfo::STATE_MAPPED_32_ZERO: 
				MoveX86RegToX86Reg(Section->MipsRegLo(count),x86Reg); 
				Section->x86Mapped(Section->MipsRegLo(count)) = CRegInfo::NotMapped;
				break;
			case CRegInfo::STATE_CONST_32:
				if (Section->MipsRegLo_S(count) < 0) { 
					CPU_Message("Sign Problems in SyncRegState\nSTATE_MAPPED_32_ZERO");
					CPU_Message("%s: %X",GPR_Name[count],Section->MipsRegLo_S(count));
#ifndef EXTERNAL_RELEASE
					DisplayError("Sign Problems in SyncRegState\nSTATE_MAPPED_32_ZERO");
#endif
				}
				MoveConstToX86reg(Section->MipsRegLo(count),x86Reg);  
				break;
#ifndef EXTERNAL_RELEASE
			default:				
				CPU_Message("Do something with states in SyncRegState\nSTATE_MAPPED_32_ZERO\n%d",Section->MipsRegState(count));
				DisplayError("Do something with states in SyncRegState\nSTATE_MAPPED_32_ZERO\n%d",Section->MipsRegState(count));
#endif
			}
			Section->MipsRegLo(count) = x86Reg;
			Section->MipsRegState(count) = SyncTo->MipsRegState(count);
			Section->x86Mapped(x86Reg) = CRegInfo::GPR_Mapped;
			Section->x86MapOrder(x86Reg) = 1;
			break;
		default:
#if (!defined(EXTERNAL_RELEASE))
			CPU_Message("%d\n%d\nreg: %s (%d)",SyncTo->MipsRegState(count),Section->MipsRegState(count),GPR_Name[count],count);
			DisplayError("%d\n%d\nreg: %s (%d)",SyncTo->MipsRegState(count),Section->MipsRegState(count),GPR_Name[count],count);
			DisplayError("Do something with states in SyncRegState");
#endif
			changed = false;
		}
	}
}

void CRecompiler::GenerateSectionLinkage (CBlockSection * Section) {
	CBlockSection ** TargetSection[2];
	CJumpInfo * JumpInfo[2];
	BYTE * Jump;
	int count;
	
	TargetSection[0] = &Section->ContinueSection;
	TargetSection[1] = &Section->JumpSection;	
	JumpInfo[0] = &Section->Cont;
	JumpInfo[1] = &Section->Jump;

	for (count = 0; count < 2; count ++) {
		if (JumpInfo[count]->LinkLocation == NULL && JumpInfo[count]->FallThrough == false) {
			JumpInfo[count]->TargetPC = -1;
		}
	}
	if ((Section->CompilePC & 0xFFC) == 0xFFC) {
		//Handle Fall througth
		Jump = NULL;
		for (count = 0; count < 2; count ++) {
			if (!JumpInfo[count]->FallThrough) { continue; }
			JumpInfo[count]->FallThrough = false;
			if (JumpInfo[count]->LinkLocation != NULL) {
				SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
				JumpInfo[count]->LinkLocation = NULL;
				if (JumpInfo[count]->LinkLocation2 != NULL) { 
					SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
					JumpInfo[count]->LinkLocation2 = NULL;
				}
			}
			MoveConstToVariable(JumpInfo[count]->TargetPC,&JumpToLocation,"JumpToLocation");
			if (JumpInfo[(count + 1) & 1]->LinkLocation == NULL) { break; }
			JmpLabel8("FinishBlock",0);
			Jump = RecompPos - 1;
		}		
		for (count = 0; count < 2; count ++) {
			if (JumpInfo[count]->LinkLocation == NULL) { continue; }
			JumpInfo[count]->FallThrough = false;
			if (JumpInfo[count]->LinkLocation != NULL) {
				SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
				JumpInfo[count]->LinkLocation = NULL;
				if (JumpInfo[count]->LinkLocation2 != NULL) { 
					SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
					JumpInfo[count]->LinkLocation2 = NULL;
				}
			}
			MoveConstToVariable(JumpInfo[count]->TargetPC,&JumpToLocation,"JumpToLocation");
			if (JumpInfo[(count + 1) & 1]->LinkLocation == NULL) { break; }
			JmpLabel8("FinishBlock",0);
			Jump = RecompPos - 1;
		}
		if (Jump != NULL) {
			CPU_Message("      $FinishBlock:");
			SetJump8(Jump,RecompPos);
		}
		MoveConstToVariable(Section->CompilePC + 4,g_PROGRAM_COUNTER,"PROGRAM_COUNTER");
		WriteBackRegisters(Section);
		g_N64System->GetRecompiler()->UpdateCounters(&Section->BlockCycleCount(),&Section->BlockRandomModifier(),false);
//		WriteBackRegisters(Section);
//		if (m_SyncSystem) { Call_Direct(SyncToPC, "SyncToPC"); }
		MoveConstToVariable(DELAY_SLOT,&NextInstruction,"NextInstruction");
		Ret();
		return;
	}
	if (!UseLinking) {  
		if (Section->CompilePC == Section->Jump.TargetPC && (Section->Cont.FallThrough == false)) {
			if (!DelaySlotEffectsJump(Section->CompilePC)) {
CPU_Message("PermLoop *** a");
				MoveConstToVariable(Section->CompilePC,g_PROGRAM_COUNTER,"PROGRAM_COUNTER");
				WriteBackRegisters(Section); 
				Section->RegWorking.BlockCycleCount() -= CountPerOp();
				g_N64System->GetRecompiler()->UpdateCounters(&Section->RegWorking.BlockCycleCount(),&Section->RegWorking.BlockRandomModifier(), false);
				Call_Direct(InPermLoop,"InPermLoop");
				Section->RegWorking.BlockCycleCount() += CountPerOp();
				g_N64System->GetRecompiler()->UpdateCounters(&Section->RegWorking.BlockCycleCount(),&Section->RegWorking.BlockRandomModifier(), true);
				g_N64System->GetRecompiler()->CompileSystemCheck(-1,Section->RegWorking);

			}
		}
	}
	if ((*TargetSection[0]) != (*TargetSection[1]) || (*TargetSection[0]) == NULL) {
		for (count = 0; count < 2; count ++) {
			if (JumpInfo[count]->LinkLocation == NULL && JumpInfo[count]->FallThrough == false) {
				if ((*TargetSection[count]))
				{
					(*TargetSection[count])->UnlinkParent(Section,true, count == 0);
				}
			} else if ((*TargetSection[count]) == NULL && JumpInfo[count]->FallThrough) {
				if (JumpInfo[count]->LinkLocation != NULL) {
					SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
					JumpInfo[count]->LinkLocation = NULL;
					if (JumpInfo[count]->LinkLocation2 != NULL) { 
						SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
						JumpInfo[count]->LinkLocation2 = NULL;
					}			
				}
				g_N64System->GetRecompiler()->CompileExit (Section,Section->CompilePC, JumpInfo[count]->TargetPC,JumpInfo[count]->RegSet,CExitInfo::Normal,true,NULL);
				JumpInfo[count]->FallThrough = false;
			} else if ((*TargetSection[count]) != NULL && JumpInfo[count] != NULL) {
				if (!JumpInfo[count]->FallThrough) { continue; }
				if (JumpInfo[count]->TargetPC == (*TargetSection[count])->StartPC) { continue; }
				if (JumpInfo[count]->LinkLocation != NULL) {
					SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
					JumpInfo[count]->LinkLocation = NULL;
					if (JumpInfo[count]->LinkLocation2 != NULL) { 
						SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
						JumpInfo[count]->LinkLocation2 = NULL;
					}			
				}
				g_N64System->GetRecompiler()->CompileExit (Section,Section->CompilePC, JumpInfo[count]->TargetPC,JumpInfo[count]->RegSet,CExitInfo::Normal,true,NULL);
				//FreeSection((*TargetSection[count]),Section);
			}
		}
	} else {
		if (Section->Cont.LinkLocation == NULL && Section->Cont.FallThrough == false) { Section->ContinueSection = NULL; }
		if (Section->Jump.LinkLocation == NULL && Section->Jump.FallThrough == false) { Section->JumpSection = NULL; }
		if (Section->JumpSection == NULL &&  Section->ContinueSection == NULL) {
			//FreeSection(TargetSection[0],Section);
		}
	}
	
	TargetSection[0] = &Section->ContinueSection;
	TargetSection[1] = &Section->JumpSection;	

	for (count = 0; count < 2; count ++) {
		if ((*TargetSection[count]) == NULL) { continue; }
		if (!JumpInfo[count]->FallThrough) { continue; }
			
		if ((*TargetSection[count])->CompiledLocation != NULL) {
			char Label[100];
			sprintf(Label,"Section_%d",(*TargetSection[count])->SectionID);
			JumpInfo[count]->FallThrough = false;
			if (JumpInfo[count]->LinkLocation != NULL) {
				SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
				JumpInfo[count]->LinkLocation = NULL;
				if (JumpInfo[count]->LinkLocation2 != NULL) { 
					SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
					JumpInfo[count]->LinkLocation2 = NULL;
				}
			}
			if (JumpInfo[count]->TargetPC <= Section->CompilePC) {
				if (JumpInfo[count]->PermLoop) {
CPU_Message("PermLoop *** 1");
					MoveConstToVariable(JumpInfo[count]->TargetPC,g_PROGRAM_COUNTER,"PROGRAM_COUNTER");
					JumpInfo[count]->RegSet.BlockCycleCount() -= CountPerOp();
					g_N64System->GetRecompiler()->UpdateCounters(&JumpInfo[count]->RegSet.BlockCycleCount(),&JumpInfo[count]->RegSet.BlockRandomModifier(), false);
					Call_Direct(InPermLoop,"InPermLoop");
					JumpInfo[count]->RegSet.BlockCycleCount() += CountPerOp();
					g_N64System->GetRecompiler()->UpdateCounters(&JumpInfo[count]->RegSet.BlockCycleCount(),&JumpInfo[count]->RegSet.BlockRandomModifier(), true);
					g_N64System->GetRecompiler()->CompileSystemCheck(-1,JumpInfo[count]->RegSet);
				} else {
					g_N64System->GetRecompiler()->UpdateCounters(&JumpInfo[count]->RegSet.BlockCycleCount(), &JumpInfo[count]->RegSet.BlockRandomModifier(), true);
					g_N64System->GetRecompiler()->CompileSystemCheck(JumpInfo[count]->TargetPC,JumpInfo[count]->RegSet);
				}
			} else {
				g_N64System->GetRecompiler()->UpdateCounters(&JumpInfo[count]->RegSet.BlockCycleCount(), &JumpInfo[count]->RegSet.BlockRandomModifier(), false);
			}
			JumpInfo[count]->RegSet.BlockRandomModifier() = 0;
			JumpInfo[count]->RegSet.BlockCycleCount() = 0;
			Section->RegWorking = JumpInfo[count]->RegSet;
			SyncRegState(Section,&(*TargetSection[count])->RegStart);						
			JmpLabel32(Label,0);
			SetJump32((DWORD *)RecompPos - 1,(*TargetSection[count])->CompiledLocation);
		}
	}
	//Section->BlockCycleCount() = 0;
	//Section->BlockRandomModifier() = 0;

	for (count = 0; count < 2; count ++) {
		if ((*TargetSection[count]) == NULL) { continue; }
		if ((*TargetSection[count])->ParentSection.empty()) { continue; }

		for (SECTION_LIST::iterator iter = (*TargetSection[count])->ParentSection.begin(); iter != (*TargetSection[count])->ParentSection.end(); iter++)
		{
			CBlockSection * Parent = *iter;

			if (Parent->CompiledLocation != NULL) { continue; }
			if (JumpInfo[count]->PermLoop) {
				CPU_Message("PermLoop *** 2");
				MoveConstToVariable(JumpInfo[count]->TargetPC,g_PROGRAM_COUNTER,"PROGRAM_COUNTER");
				JumpInfo[count]->RegSet.BlockCycleCount() -= CountPerOp();
				g_N64System->GetRecompiler()->UpdateCounters(&JumpInfo[count]->RegSet.BlockCycleCount(),&JumpInfo[count]->RegSet.BlockRandomModifier(), false);
				Call_Direct(InPermLoop,"InPermLoop");
				JumpInfo[count]->RegSet.BlockCycleCount() += CountPerOp();
				g_N64System->GetRecompiler()->UpdateCounters(&JumpInfo[count]->RegSet.BlockCycleCount(),&JumpInfo[count]->RegSet.BlockRandomModifier(), true);
				g_N64System->GetRecompiler()->CompileSystemCheck(-1,JumpInfo[count]->RegSet);
			}
			if (JumpInfo[count]->FallThrough) { 
				JumpInfo[count]->FallThrough = false;
				JmpLabel32(JumpInfo[count]->BranchLabel,0);
				JumpInfo[count]->LinkLocation = RecompPos - 4;
			}
		}
	}

	for (count = 0; count < 2; count ++) {
		if (JumpInfo[count]->FallThrough) { 
			if (JumpInfo[count]->TargetPC < Section->CompilePC) {
				g_N64System->GetRecompiler()->UpdateCounters(&JumpInfo[count]->RegSet.BlockCycleCount(),&JumpInfo[count]->RegSet.BlockRandomModifier(),true);
				g_N64System->GetRecompiler()->CompileSystemCheck(JumpInfo[count]->TargetPC,JumpInfo[count]->RegSet);
			}
		}
	}

	CPU_Message("====== End of Section %d ======",Section->SectionID);

	for (count = 0; count < 2; count ++) {
		if (JumpInfo[count]->FallThrough) { 
			g_N64System->GetRecompiler()->GenerateX86Code(*(Section->BlockInfo),(*TargetSection[count]),CBlockSection::GetNewTestValue()); 
		}
	}
	
	//CPU_Message("Section %d",Section->SectionID);
	for (count = 0; count < 2; count ++) {
		if (JumpInfo[count]->LinkLocation == NULL) { continue; }
		if ((*TargetSection[count]) == NULL) {
			CPU_Message("ExitBlock (from %d):",Section->SectionID);
			SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
			JumpInfo[count]->LinkLocation = NULL;
			if (JumpInfo[count]->LinkLocation2 != NULL) { 
				SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
				JumpInfo[count]->LinkLocation2 = NULL;
			}			
			g_N64System->GetRecompiler()->CompileExit (Section,Section->CompilePC,JumpInfo[count]->TargetPC,JumpInfo[count]->RegSet,CExitInfo::Normal,true,NULL);
			continue;
		}
		if (JumpInfo[count]->TargetPC != (*TargetSection[count])->StartPC) {
			DisplayError("I need to add more code in GenerateSectionLinkage cause this is going to cause an exception");
			BreakPoint(__FILE__,__LINE__); 
		}
		if ((*TargetSection[count])->CompiledLocation == NULL) {
			g_N64System->GetRecompiler()->GenerateX86Code(*(*TargetSection[count])->BlockInfo,(*TargetSection[count]),CBlockSection::GetNewTestValue()); 
		} else {
			char Label[100];

			CPU_Message("Section_%d (from %d):",(*TargetSection[count])->SectionID,Section->SectionID);
			SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
			JumpInfo[count]->LinkLocation = NULL;
			if (JumpInfo[count]->LinkLocation2 != NULL) { 
				SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
				JumpInfo[count]->LinkLocation2 = NULL;
			}			
			Section->RegWorking = JumpInfo[count]->RegSet;
			if (JumpInfo[count]->TargetPC <= Section->CompilePC) {
				g_N64System->GetRecompiler()->UpdateCounters(&JumpInfo[count]->RegSet.BlockCycleCount(),&JumpInfo[count]->RegSet.BlockRandomModifier(), true);

				if (JumpInfo[count]->PermLoop) {
CPU_Message("PermLoop *** 3");
					MoveConstToVariable(JumpInfo[count]->TargetPC,g_PROGRAM_COUNTER,"PROGRAM_COUNTER");
					Call_Direct(InPermLoop,"InPermLoop");
					g_N64System->GetRecompiler()->CompileSystemCheck(-1,JumpInfo[count]->RegSet);
				} else {
					g_N64System->GetRecompiler()->CompileSystemCheck(JumpInfo[count]->TargetPC,JumpInfo[count]->RegSet);
				}
			} else{
				g_N64System->GetRecompiler()->UpdateCounters(&JumpInfo[count]->RegSet.BlockCycleCount(),&JumpInfo[count]->RegSet.BlockRandomModifier(), false);
			}
			Section->RegWorking = JumpInfo[count]->RegSet;
			SyncRegState(Section,&(*TargetSection[count])->RegStart);						
			JmpLabel32(Label,0);
			SetJump32((DWORD *)RecompPos - 1,(*TargetSection[count])->CompiledLocation);
		}
	}
}

void CRecompiler::RemoveFunction (FUNCTION_INFO * FunInfo, bool DelaySlot, REMOVE_REASON Reason )
{
	LPCSTR lpReason = "Unknown";
	switch (Reason)
	{
	case Remove_InitialCode: lpReason = "Remove_InitialCode"; break;
	case Remove_Cache:       lpReason = "Remove_Cache"; break;
	case Remove_ProtectedMem: lpReason = "Remove_ProtectedMem"; break;
	case Remove_ValidateFunc: lpReason = "Remove_ValidateFunc"; break;
	case Remove_TLB: lpReason = "Remove_TLB"; break;
	case Remove_DMA: lpReason = "Remove_DMA"; break;
	}
	WriteTraceF(TraceRecompiler,"Remove Func (%s): %X-%X",lpReason, FunInfo->VStartPC(), FunInfo->VEndPC());
	if (JumpTable && !DelaySlot)
	{
		FUNCTION_INFO * Info = FunInfo;
		while (Info)
		{
			if (*(JumpTable + (Info->PStartPC() >> 2)) == Info)
			{
				*(JumpTable + (Info->PStartPC() >> 2)) = NULL;
			}
			Info = Info->Next;

		}
	}
/*	
	TlbLog.Log("Clear %X",FunInfo->StartPC());

	//Remove from the list of called functions
	FunInfo->RemoveFromCallList();

	//remove all functions that call this
	if (_Settings->LoadDword(SMM_Linked)) {
		for (FUNCTION_PTR_MAP::iterator item = FunInfo->CallingList().begin(); 
			FunInfo->CallingList().begin() != FunInfo->CallingList().end(); 
			item = FunInfo->CallingList().begin())
		{
			RemoveFunction(m_Functions.find(item->first));
		}
	} else {
		//Remove from the list of called functions
		FunInfo->RemoveFromCallingList();
	}
*/
	DWORD StartBlock = FunInfo->VStartPC() & ~0xFFF;
	DWORD EndBlock   = FunInfo->VEndPC() & ~0xFFF;

	//Remove this item from the main list
	if (DelaySlot)
	{
		m_FunctionsDelaySlot.Remove(FunInfo);
	} else {
		m_Functions.Remove(FunInfo); 
	}

	//if no more functions in this block then unprotect the memory
	if (bSMM_Protect()) 
	{
		for (DWORD Addr = StartBlock; Addr <= EndBlock; Addr += 0x1000 ){
			FUNCTION_INFO * info = m_Functions.FindFunction(Addr,0xFFF);
			if (info == NULL)
			{
				_MMU->UnProtectMemory(Addr,Addr + 0xFFC);
			}
		}
	}
}

bool CRecompiler::ClearRecompCode_Phys(DWORD Address, int length, REMOVE_REASON Reason ) {
	bool Result = true;
	if (!ClearRecompCode_Virt(Address + 0x80000000,length,Reason)) { Result = false; }
	if (!ClearRecompCode_Virt(Address + 0xA0000000,length,Reason)) { Result = false; }

	if (g_UseTlb)
	{
		DWORD VAddr, Index = 0;
		while (_MMU->PAddrToVAddr(Address,VAddr,Index))
		{
			WriteTraceF(TraceRecompiler,"ClearRecompCode Vaddr %X  len: %d",VAddr,length);
			if (!ClearRecompCode_Virt(VAddr,length,Reason))
			{
				Result = false; 
			}
		}
	}
	if (LookUpMode() == FuncFind_PhysicalLookup) 
	{
		WriteTraceF(TraceRecompiler,"Reseting Jump Table, Addr: %X  len: %d",Address,((length + 3) & ~3));
		memset((BYTE *)JumpTable + Address,0,((length + 3) & ~3));
	}

	return Result;
}

bool CRecompiler::ClearRecompCode_Virt(DWORD Address, int length,REMOVE_REASON Reason ) 
{
	FUNCTION_INFO * info; 
	do 
	{
		info = m_Functions.FindFunction(Address,length);
		if (info)
		{
			RemoveFunction(info,false,Reason);
		}
	} while (info != NULL);
	do 
	{
		info = m_FunctionsDelaySlot.FindFunction(Address,length);
		if (info)
		{
			RemoveFunction(info,true,Reason);
		}
	} while (info != NULL);

	if (bSMM_Protect())
	{
		DWORD Start = Address  & ~0xFFF;
		info = m_Functions.FindFunction(Start,0xFFF);
		if (info) 
		{
			WriteTraceF(TraceDebug,"Function exists at %X End: %X",info->VStartPC(),info->VEndPC());
			return false; 
		}
		info = m_FunctionsDelaySlot.FindFunction(Start,0xFFF);
		if (info) 
		{
			WriteTraceF(TraceDebug,"Delay function exists at %X End: %X",info->VStartPC(),info->VEndPC());
			return false; 
		}
		return true;
	}
	return true;
}

void CRecompiler::SyncRegState (CBlockSection * Section, CRegInfo * SyncTo) 
{
	int count, x86Reg,x86RegHi;
	
	bool changed = FALSE;
	UnMap_AllFPRs(Section);
	if (Section->CurrentRoundingModel() != SyncTo->CurrentRoundingModel()) { Section->CurrentRoundingModel() = CRegInfo::RoundUnknown; }
	
	//x86Reg = Map_MemoryStack(Section, x86_Any, FALSE);
	//CPU_Message("MemoryStack for Original State = %s",x86Reg > 0?x86_Name(x86Reg):"Not Mapped");

	for (x86Reg = 1; x86Reg < 10; x86Reg ++) {
		if (Section->x86Mapped(x86Reg) != CRegInfo::Stack_Mapped) { continue; }
		if (SyncTo->x86Mapped(x86Reg) != CRegInfo::Stack_Mapped) {
			UnMap_X86reg(Section,x86Reg);
			for (count = 1; count < 10; count ++) {
				if (SyncTo->x86Mapped(count) == CRegInfo::Stack_Mapped) {
					MoveX86RegToX86Reg(count,x86Reg); 
					changed = TRUE;
				}
			}
			if (!changed) {
				MoveVariableToX86reg(g_MemoryStack,"MemoryStack",x86Reg);
			}
			changed = TRUE;
		}
	}
	for (x86Reg = 1; x86Reg < 10; x86Reg ++) {
		if (SyncTo->x86Mapped(x86Reg) != CRegInfo::Stack_Mapped) { continue; }
		//CPU_Message("MemoryStack for Sync State = %s",x86Reg > 0?x86_Name(x86Reg):"Not Mapped");
		if (Section->x86Mapped(x86Reg) == CRegInfo::Stack_Mapped) { break; }
		UnMap_X86reg(Section,x86Reg);		
	}
	
	for (count = 1; count < 32; count ++) {
		if (Section->MipsRegState(count) == SyncTo->MipsRegState(count)) {
			switch (Section->MipsRegState(count)) {
			case CRegInfo::STATE_UNKNOWN: continue;
			case CRegInfo::STATE_MAPPED_64:
				if (Section->MipsReg(count) == SyncTo->MipsReg(count)) {
					continue;
				}
				break;
			case CRegInfo::STATE_MAPPED_32_ZERO:
			case CRegInfo::STATE_MAPPED_32_SIGN:
				if (Section->MipsRegLo(count) == SyncTo->MipsRegLo(count)) {
					continue;
				}
				break;
			case CRegInfo::STATE_CONST_64:
				if (Section->MipsReg(count) != SyncTo->MipsReg(count)) {
#if (!defined(EXTERNAL_RELEASE))
					DisplayError("Umm.. how ???");
#endif
				}
				continue;
			case CRegInfo::STATE_CONST_32:
				if (Section->MipsRegLo(count) != SyncTo->MipsRegLo(count)) {
#if (!defined(EXTERNAL_RELEASE))
					DisplayError("Umm.. how ???");
#endif
				}
				continue;
#ifndef EXTERNAL_RELEASE
			default:
				DisplayError("Unhandled Reg state %d\nin SyncRegState",Section->MipsRegState(count));
#endif
			}			
		}
		changed = TRUE;

		switch (SyncTo->MipsRegState(count)) {
		case CRegInfo::STATE_UNKNOWN: UnMap_GPR(Section,count,TRUE);  break;
		case CRegInfo::STATE_MAPPED_64:
			x86Reg = SyncTo->MipsRegLo(count);
			x86RegHi = SyncTo->MipsRegHi(count);
			UnMap_X86reg(Section,x86Reg);
			UnMap_X86reg(Section,x86RegHi);
			switch (Section->MipsRegState(count)) {
			case CRegInfo::STATE_UNKNOWN:
				MoveVariableToX86reg(&GPR[count].UW[0],GPR_NameLo[count],x86Reg);
				MoveVariableToX86reg(&GPR[count].UW[1],GPR_NameHi[count],x86RegHi);
				break;
			case CRegInfo::STATE_MAPPED_64:
				MoveX86RegToX86Reg(Section->MipsRegLo(count),x86Reg); 
				Section->x86Mapped(Section->MipsRegLo(count)) = CRegInfo::NotMapped;
				MoveX86RegToX86Reg(Section->MipsRegHi(count),x86RegHi); 
				Section->x86Mapped(Section->MipsRegHi(count)) = CRegInfo::NotMapped;
				break;
			case CRegInfo::STATE_MAPPED_32_SIGN:
				MoveX86RegToX86Reg(Section->MipsRegLo(count),x86RegHi); 
				ShiftRightSignImmed(x86RegHi,31);
				MoveX86RegToX86Reg(Section->MipsRegLo(count),x86Reg); 
				Section->x86Mapped(Section->MipsRegLo(count)) = CRegInfo::NotMapped;
				break;
			case CRegInfo::STATE_MAPPED_32_ZERO:
				XorX86RegToX86Reg(x86RegHi,x86RegHi);
				MoveX86RegToX86Reg(Section->MipsRegLo(count),x86Reg); 
				Section->x86Mapped(Section->MipsRegLo(count)) = CRegInfo::NotMapped;
				break;
			case CRegInfo::STATE_CONST_64:
				MoveConstToX86reg(Section->MipsRegHi(count),x86RegHi); 
				MoveConstToX86reg(Section->MipsRegLo(count),x86Reg); 
				break;
			case CRegInfo::STATE_CONST_32:
				MoveConstToX86reg(Section->MipsRegLo_S(count) >> 31,x86RegHi); 
				MoveConstToX86reg(Section->MipsRegLo(count),x86Reg); 
				break;
			default:
#ifndef EXTERNAL_RELEASE
				CPU_Message("Do something with states in SyncRegState\nSTATE_MAPPED_64\n%d",Section->MipsRegState(count));
				DisplayError("Do something with states in SyncRegState\nSTATE_MAPPED_64\n%d",Section->MipsRegState(count));
#endif
				continue;
			}
			Section->MipsRegLo(count) = x86Reg;
			Section->MipsRegHi(count) = x86RegHi;
			Section->MipsRegState(count) = CRegInfo::STATE_MAPPED_64;
			Section->x86Mapped(x86Reg) = CRegInfo::GPR_Mapped;
			Section->x86Mapped(x86RegHi) = CRegInfo::GPR_Mapped;
			Section->x86MapOrder(x86Reg) = 1;
			Section->x86MapOrder(x86RegHi) = 1;
			break;
		case CRegInfo::STATE_MAPPED_32_SIGN:
			x86Reg = SyncTo->MipsRegLo(count);
			UnMap_X86reg(Section,x86Reg);
			switch (Section->MipsRegState(count)) {
			case CRegInfo::STATE_UNKNOWN: MoveVariableToX86reg(&GPR[count].UW[0],GPR_NameLo[count],x86Reg); break;
			case CRegInfo::STATE_CONST_32: MoveConstToX86reg(Section->MipsRegLo(count),x86Reg); break;
			case CRegInfo::STATE_MAPPED_32_SIGN: 
				MoveX86RegToX86Reg(Section->MipsRegLo(count),x86Reg); 
				Section->x86Mapped(Section->MipsRegLo(count)) = CRegInfo::NotMapped;
				break;
			case CRegInfo::STATE_MAPPED_32_ZERO:
				if (Section->MipsRegLo(count) != (DWORD)x86Reg) {
					MoveX86RegToX86Reg(Section->MipsRegLo(count),x86Reg); 
					Section->x86Mapped(Section->MipsRegLo(count)) = CRegInfo::NotMapped;
				}
				break;
			case CRegInfo::STATE_MAPPED_64:
				MoveX86RegToX86Reg(Section->MipsRegLo(count),x86Reg); 
				Section->x86Mapped(Section->MipsRegLo(count)) = CRegInfo::NotMapped;
				Section->x86Mapped(Section->MipsRegHi(count)) = CRegInfo::NotMapped;
				break;
#ifndef EXTERNAL_RELEASE
			case CRegInfo::STATE_CONST_64:
				DisplayError("hi %X\nLo %X",Section->MipsRegHi(count),Section->MipsRegLo(count));
			default:				
				CPU_Message("Do something with states in SyncRegState\nSTATE_MAPPED_32_SIGN\n%d",Section->MipsRegState(count));
				DisplayError("Do something with states in SyncRegState\nSTATE_MAPPED_32_SIGN\n%d",Section->MipsRegState(count));
#endif
			}
			Section->MipsRegLo(count) = x86Reg;
			Section->MipsRegState(count) = CRegInfo::STATE_MAPPED_32_SIGN;
			Section->x86Mapped(x86Reg) = CRegInfo::GPR_Mapped;
			Section->x86MapOrder(x86Reg) = 1;
			break;
		case CRegInfo::STATE_MAPPED_32_ZERO:
			x86Reg = SyncTo->MipsRegLo(count);
			UnMap_X86reg(Section,x86Reg);
			switch (Section->MipsRegState(count)) {
			case CRegInfo::STATE_MAPPED_64:
			case CRegInfo::STATE_UNKNOWN:  
				MoveVariableToX86reg(&GPR[count].UW[0],GPR_NameLo[count],x86Reg); 
				break;
			case CRegInfo::STATE_MAPPED_32_ZERO: 
				MoveX86RegToX86Reg(Section->MipsRegLo(count),x86Reg); 
				Section->x86Mapped(Section->MipsRegLo(count)) = CRegInfo::NotMapped;
				break;
			case CRegInfo::STATE_CONST_32:
				if (Section->MipsRegLo_S(count) < 0) { 
					CPU_Message("Sign Problems in SyncRegState\nSTATE_MAPPED_32_ZERO");
					CPU_Message("%s: %X",GPR_Name[count],Section->MipsRegLo_S(count));
#ifndef EXTERNAL_RELEASE
					DisplayError("Sign Problems in SyncRegState\nSTATE_MAPPED_32_ZERO");
#endif
				}
				MoveConstToX86reg(Section->MipsRegLo(count),x86Reg);  
				break;
#ifndef EXTERNAL_RELEASE
			default:				
				CPU_Message("Do something with states in SyncRegState\nSTATE_MAPPED_32_ZERO\n%d",Section->MipsRegState(count));
				DisplayError("Do something with states in SyncRegState\nSTATE_MAPPED_32_ZERO\n%d",Section->MipsRegState(count));
#endif
			}
			Section->MipsRegLo(count) = x86Reg;
			Section->MipsRegState(count) = SyncTo->MipsRegState(count);
			Section->x86Mapped(x86Reg) = CRegInfo::GPR_Mapped;
			Section->x86MapOrder(x86Reg) = 1;
			break;
		default:
#if (!defined(EXTERNAL_RELEASE))
			CPU_Message("%d\n%d\nreg: %s (%d)",SyncTo->MipsRegState(count),Section->MipsRegState(count),GPR_Name[count],count);
			DisplayError("%d\n%d\nreg: %s (%d)",SyncTo->MipsRegState(count),Section->MipsRegState(count),GPR_Name[count],count);
			DisplayError("Do something with states in SyncRegState");
#endif
			changed = FALSE;
		}
	}
}