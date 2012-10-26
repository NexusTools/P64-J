#include "../../User Interface.h"
#include "../../User Interface/WTL Controls/ModifiedCheckBox.h"
#include "../../User Interface/WTL Controls/ModifiedEditBox.h"
#include "../../User Interface/WTL Controls/ModifiedComboBox.h"
#include "../../User Interface/WTL Controls/PartialGroupBox.h"
#include "../../User Interface/Settings Config.h"
#include "Settings Page.h"
#include "Settings Page - Game - General.h"

CGameGeneralPage::CGameGeneralPage (HWND hParent, const RECT & rcDispay )
{
	if (!Create(hParent,rcDispay))
	{
		return;
	}

#ifdef tofix
	AddModCheckBox(GetDlgItem(IDC_ROM_32BIT),ROM_32Bit);
	AddModCheckBox(GetDlgItem(IDC_SYNC_AUDIO),ROM_SyncViaAudio);
#endif
	AddModCheckBox(GetDlgItem(IDC_ROM_FIXEDAUDIO),ROM_FixedAudio);
	AddModCheckBox(GetDlgItem(IDC_USE_TLB),ROM_UseTlb);
#ifdef tofix
	AddModCheckBox(GetDlgItem(IDC_DELAY_DP),ROM_DelayDP);
#endif
	AddModCheckBox(GetDlgItem(IDC_DELAY_SI),ROM_DelaySI);
#ifdef tofix
	AddModCheckBox(GetDlgItem(IDC_AUDIO_SIGNAL),ROM_RspAudioSignal);
#endif

	CModifiedComboBox * ComboBox;
	ComboBox = AddModComboBox(GetDlgItem(IDC_RDRAM_SIZE),ROM_RamSize);
	if (ComboBox)
	{
		ComboBox->SetTextField(GetDlgItem(IDC_MEMORY_SIZE_TEXT));
		ComboBox->AddItem(GS(RDRAM_4MB), 0x400000 );
		ComboBox->AddItem(GS(RDRAM_8MB), 0x800000 );
	}

	ComboBox = AddModComboBox(GetDlgItem(IDC_SAVE_TYPE),ROM_SaveChip);
	if (ComboBox)
	{
		ComboBox->SetTextField(GetDlgItem(IDC_SAVE_TYPE_TEXT));
		ComboBox->AddItem(GS(SAVE_FIRST_USED), (WPARAM)SaveChip_Auto );
		ComboBox->AddItem(GS(SAVE_4K_EEPROM),  SaveChip_Eeprom_4K );
		ComboBox->AddItem(GS(SAVE_16K_EEPROM), SaveChip_Eeprom_16K );
		ComboBox->AddItem(GS(SAVE_SRAM),       SaveChip_Sram );
		ComboBox->AddItem(GS(SAVE_FLASHRAM),   SaveChip_FlashRam );
	}

	ComboBox = AddModComboBox(GetDlgItem(IDC_COUNTFACT),ROM_CounterFactor);
	if (ComboBox)
	{
		ComboBox->SetTextField(GetDlgItem(IDC_COUNTFACT_TEXT));
		ComboBox->AddItem(GS(NUMBER_1), 1 );
		ComboBox->AddItem(GS(NUMBER_2), 2 );
		ComboBox->AddItem(GS(NUMBER_3), 3 );
		ComboBox->AddItem(GS(NUMBER_4), 4 );
		ComboBox->AddItem(GS(NUMBER_5), 5 );
		ComboBox->AddItem(GS(NUMBER_6), 6 );
	}

	SetDlgItemText(IDC_GOOD_NAME,_Settings->LoadString(ROM_GoodName).c_str());

#ifdef tofix
	CModifiedEditBox * TxtBox = AddModTextBox(GetDlgItem(IDC_VIREFRESH),ROM_ViRefreshRate, false);
	TxtBox->SetTextField(GetDlgItem(IDC_VIREFESH_TEXT));
#endif

	UpdatePageSettings();
}

void CGameGeneralPage::ShowPage()
{
	ShowWindow(SW_SHOW);
}

void CGameGeneralPage::HidePage()
{
	ShowWindow(SW_HIDE);
}

void CGameGeneralPage::ApplySettings( bool UpdateScreen )
{
	CSettingsPageImpl<CGameGeneralPage>::ApplySettings(UpdateScreen);
}

bool CGameGeneralPage::EnableReset ( void )
{
	if (CSettingsPageImpl<CGameGeneralPage>::EnableReset()) { return true; }
	return false;
}


void CGameGeneralPage::ResetPage()
{
	CSettingsPageImpl<CGameGeneralPage>::ResetPage();
}