#include "../../User Interface.h"
#include "../../User Interface/WTL Controls/ModifiedCheckBox.h"
#include "../../User Interface/WTL Controls/ModifiedEditBox.h"
#include "../../User Interface/WTL Controls/ModifiedComboBox.h"
#include "../../User Interface/WTL Controls/PartialGroupBox.h"
#include "../../User Interface/Settings Config.h"
#include "Settings Page.h"
#include "Settings Page - Game - Recompiler.h"

CGameRecompilePage::CGameRecompilePage (HWND hParent, const RECT & rcDispay )
{
	if (!Create(hParent,rcDispay))
	{
		return;
	}

	m_SelfModGroup.Attach(GetDlgItem(IDC_SMM_FRAME));

	AddModCheckBox(GetDlgItem(IDC_ROM_REGCACHE),ROM_RegCache);
	AddModCheckBox(GetDlgItem(IDC_BLOCK_LINKING),ROM_BlockLinking);
	AddModCheckBox(GetDlgItem(IDC_SMM_CACHE),ROM_SMM_Cache);
	AddModCheckBox(GetDlgItem(IDC_SMM_DMA),ROM_SMM_PIDMA);
	AddModCheckBox(GetDlgItem(IDC_SMM_VALIDATE),ROM_SMM_ValidFunc);
	AddModCheckBox(GetDlgItem(IDC_SMM_TLB),ROM_SMM_TLB);
	AddModCheckBox(GetDlgItem(IDC_SMM_PROTECT),ROM_SMM_Protect);
	::ShowWindow(GetDlgItem(IDC_SMM_STORE),SW_HIDE);
	//AddModCheckBox(GetDlgItem(IDC_SMM_STORE),ROM_SMM_StoreInstruc);
	AddModCheckBox(GetDlgItem(IDC_ROM_FASTSP),ROM_SPHack);

	CModifiedComboBox * ComboBox;
	ComboBox = AddModComboBox(GetDlgItem(IDC_CPU_TYPE),CPUType);
	if (ComboBox)
	{
		ComboBox->AddItem(GS(CORE_RECOMPILER), CPU_Recompiler);
		ComboBox->AddItem(GS(CORE_INTERPTER), CPU_Interpreter);
#ifdef tofix
		if (_Settings->LoadBool(Debugger_Enabled))
		{
			ComboBox->AddItem(GS(CORE_SYNC), CPU_SyncCores);
		}
#endif
	}

	ComboBox = AddModComboBox(GetDlgItem(IDC_FUNCFIND),FuncLookupMode);
	if (ComboBox)
	{
		ComboBox->AddItem(GS(FLM_PLOOKUP), FuncFind_PhysicalLookup);
		ComboBox->AddItem(GS(FLM_VLOOKUP), FuncFind_VirtualLookup);
		//ComboBox->AddItem(GS(FLM_CHANGEMEM), FuncFind_ChangeMemory);
	}
	UpdatePageSettings();
}

void CGameRecompilePage::ShowPage()
{
	ShowWindow(SW_SHOW);
}

void CGameRecompilePage::HidePage()
{
	ShowWindow(SW_HIDE);
}

void CGameRecompilePage::ApplySettings( bool UpdateScreen )
{
	CSettingsPageImpl<CGameRecompilePage>::ApplySettings(UpdateScreen);
}

bool CGameRecompilePage::EnableReset ( void )
{
	if (CSettingsPageImpl<CGameRecompilePage>::EnableReset()) { return true; }
	return false;
}

void CGameRecompilePage::ResetPage()
{
	CSettingsPageImpl<CGameRecompilePage>::ResetPage();
}
