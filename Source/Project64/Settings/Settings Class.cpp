#include "..\N64 System.h"
#include "..\Settings.h"
#include <common/CriticalSection.h>
#include "SettingType/SettingsType-Application.h"
#include "SettingType/SettingsType-ApplicationIndex.h"
#include "SettingType/SettingsType-GameSetting.h"
#include "SettingType/SettingsType-RomDatabase.h"
#include "SettingType/SettingsType-RelativePath.h"
#include "SettingType/SettingsType-TempString.h"
#include "SettingType/SettingsType-TempNumber.h"
#include "SettingType/SettingsType-TempBool.h"

CSettings * _Settings = NULL;

CSettings::CSettings()
{
}

CSettings::~CSettings()
{
	for (SETTING_MAP::iterator iter = m_SettingInfo.begin(); iter != m_SettingInfo.end(); iter++)
	{
		delete iter->second;
	}

	for (SETTING_CALLBACK::iterator cb_iter = m_Callback.begin(); cb_iter != m_Callback.end(); cb_iter++)
	{

		SETTING_CHANGED_CB * item = cb_iter->second;
		while (item != NULL)
		{
			SETTING_CHANGED_CB * current_item = item;
			item = item->Next;
			delete current_item;
		}
	}

	CSettingTypeApplication::CleanUp();
	CSettingTypeRomDatabase::CleanUp();

}

void CSettings::AddHandler ( SettingID TypeID, CSettingType * Handler )
{
	m_SettingInfo.insert(SETTING_MAP::value_type(TypeID,Handler));
}

void CSettings::AddHowToHandleSetting ()
{
	//Support Files
	AddHandler(SettingsIniName, new CSettingTypeRelativePath("","Project64.cfg"));
	AddHandler(RomDatabaseFile, new CSettingTypeRelativePath("","Project64.rdb"));
	AddHandler(CheatIniName,    new CSettingTypeRelativePath("","Project64.cht"));
	AddHandler(NotesIniName,    new CSettingTypeRelativePath("","Project64.rdn"));
	AddHandler(ExtIniName,      new CSettingTypeRelativePath("","Project64.rdx"));
	AddHandler(ShortCutFile,    new CSettingTypeRelativePath("","Project64.sc2"));
	AddHandler(RomListCache,    new CSettingTypeRelativePath("","Project64.cache3"));
	AddHandler(ZipCacheIniName, new CSettingTypeRelativePath("","Project64.zcache"));
	AddHandler(SyncPluginDir,   new CSettingTypeRelativePath("SyncPlugin",""));

	//Settings location
	AddHandler(UseSettingFromRegistry,new CSettingTypeApplication("Settings","Use Registry",(DWORD)false));

/*	INFO(SettingsIniName,No_Default,Data_String,RelativePath,"Project64.cfg","",0);
	if (SettingsIniFile == NULL)
	{
		SettingsIniFile = new CIniFile(LoadString(SettingsIniName).c_str());
	}

	INFO(UseSettingFromRegistry,Default_False,Data_DWORD,LocalSettings,"Use Registry","Settings",0);
	SettingLocation SettingLoc = LoadDword(UseSettingFromRegistry) ? InRegistry : LocalSettings;
*/

	/*	int count;

#define INFO(ID,X,Y,Z,Q,W,E) SettingInfo.insert(SETTING_MAP::value_type(ID,CSettingInfo(ID,X,Y,Z,Q,W,E)))
#define INF2(ID,X,Y,Z,Q,W,E,R) SettingInfo.insert(SETTING_MAP::value_type(ID,CSettingInfo(ID,X,Y,Z,Q,W,E,R)))
	//Default Values
	INFO(Default_False,               No_Default, Data_DWORD,  ConstValue,  "","",(DWORD)false);
	INFO(Default_True,                No_Default, Data_DWORD,  ConstValue,  "","",(DWORD)true);
	INFO(Default_Language,            No_Default, Data_String, ConstString, "","",0);
	INFO(Default_RomStatus,           No_Default, Data_String, ConstString, "Unknown","",0);
	INFO(Default_RomBrowserWidth,     No_Default, Data_DWORD,  ConstValue,  "","",640);
	INFO(Default_RomBrowserHeight,    No_Default, Data_DWORD,  ConstValue,  "","",480);
	INFO(Default_RememberedRomFiles,  No_Default, Data_DWORD,  ConstValue,  "","",MaxRememberedFiles);
	INFO(Default_RememberedRomDirs,   No_Default, Data_DWORD,  ConstValue,  "","",MaxRememberedDirs);
	INFO(Default_CPUType,             No_Default, Data_DWORD,  ConstValue,  "","",CPU_Recompiler);
	INFO(Default_RdramSize,           No_Default, Data_DWORD,  ConstValue,  "","",0x400000);
	INFO(Default_SaveChip,            No_Default, Data_DWORD,  ConstValue,  "","",SaveChip_Auto);
	INFO(Default_CFactor,             No_Default, Data_DWORD,  ConstValue,  "","",2);
	INFO(Default_CheatExt,            No_Default, Data_String, ConstString, "?","",0);
	INFO(Default_FunctionLookup,      No_Default, Data_DWORD,  ConstValue,  "","",FuncFind_PhysicalLookup);
	INFO(Default_BlockLinking,        No_Default, Data_DWORD,  ConstValue,  "","",(DWORD)false);
	INFO(Default_SaveSlot,            No_Default, Data_DWORD,  ConstValue,  "","",(DWORD)0);
	INFO(Default_LogLevel,            No_Default, Data_DWORD,  ConstValue,  "","",(DWORD)TraceError);
	INFO(Default_FrameDisplayType,    No_Default, Data_DWORD,  ConstValue,  "","",FR_VIs);

	
	//Add setting to see if we get settings from file system or registry
	INFO(SettingsIniName,No_Default,Data_String,RelativePath,"Project64.cfg","",0);
	if (SettingsIniFile == NULL)
	{
		SettingsIniFile = new CIniFile(LoadString(SettingsIniName).c_str());
	}

	INFO(UseSettingFromRegistry,Default_False,Data_DWORD,LocalSettings,"Use Registry","Settings",0);
	SettingLocation SettingLoc = LoadDword(UseSettingFromRegistry) ? InRegistry : LocalSettings;
*/
	//Language
	AddHandler(CurrentLanguage,new CSettingTypeApplication("","Current Language",""));

	//Gui Settings
	AddHandler(RomBrowser,          new CSettingTypeApplication("Rom Browser","Rom Browser",true));
	AddHandler(MainWindowTop,       new CSettingTypeApplication("Main Window","Top"        ,No_Default));
	AddHandler(MainWindowLeft,      new CSettingTypeApplication("Main Window","Left"       ,No_Default));
	AddHandler(RomBrowserTop,       new CSettingTypeApplication("Rom Browser","Top"        ,No_Default));
	AddHandler(RomBrowserLeft,      new CSettingTypeApplication("Rom Browser","Left"       ,No_Default));
	AddHandler(RomBrowserHeight,    new CSettingTypeApplication("Rom Browser","Height",    (DWORD)480));
	AddHandler(RomBrowserWidth,     new CSettingTypeApplication("Rom Browser","Width",     (DWORD)640));
	AddHandler(RomBrowserRecursive, new CSettingTypeApplication("Rom Browser","Recursive", false));
	AddHandler(RomBrowserMaximized, new CSettingTypeApplication("Rom Browser","Maximized", false));
	AddHandler(RomBrowserSortFieldIndex, new CSettingTypeApplicationIndex("Rom Browser", "Sort Field",  No_Default));
	AddHandler(RomBrowserPosIndex,  new CSettingTypeApplicationIndex("Rom Browser\\Field Pos","Field",(DWORD)0));
	AddHandler(RomBrowserWidthIndex,new CSettingTypeApplicationIndex("Rom Browser\\Field Width","Field",(DWORD)100));

	/*
	INFO(,Default_False,Data_DWORD,SettingLoc,"","Rom Browser",0);
	INFO(,Default_False,Data_DWORD,SettingLoc,"","Rom Browser",0);
	for (int SortID = 0; SortID <= NoOfSortKeys; SortID++ ) {
		char Name[300];
		_snprintf(Name,sizeof(Name),"Sort Field %d",SortID);
		INF2((SettingID)(SortField + SortID),No_Default,Data_String,SettingLoc,Name,"Rom Browser",0,SortField);
		_snprintf(Name,sizeof(Name),"Sort Ascending %d",SortID);
		INF2((SettingID)(SortAscending + SortID),Default_True,Data_DWORD,SettingLoc,Name,"Rom Browser",0,SortAscending);
	}
	for (int Field = 0; Field <= MaxRomBrowserFields; Field++ ) {
		char Name[300];
		_snprintf(Name,sizeof(Name),"Field %02d",Field);
		INF2((SettingID)(FirstRomBrowserPos + Field),No_Default,Data_DWORD,SettingLoc,Name,"Rom Browser\\Field Pos",0,FirstRomBrowserPos);
		INF2((SettingID)(FirstRomBrowserWidth + Field),No_Default,Data_DWORD,SettingLoc,Name,"Rom Browser\\Field Width",0,FirstRomBrowserWidth);
	}
	INFO(TLBWindowTop,No_Default,Data_DWORD,SettingLoc,"Rom Browser Top","Page Setup",0);
	INFO(TLBWindowLeft,No_Default,Data_DWORD,SettingLoc,"Rom Browser Left","Page Setup",0);
*/

	
	//Beta settings
	AddHandler(IsBetaVersion,       new CSettingTypeTempBool(true));
	AddHandler(BetaUserName,        new CSettingTypeTempString("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"));
	AddHandler(BetaEmailAddress,    new CSettingTypeTempString("????????????????????????????????????????????????????????????????????????????????"));

	//General Settings
	AddHandler(AutoStart,        new CSettingTypeApplication("","Auto Start",          (DWORD)true));
	AddHandler(AutoZip,          new CSettingTypeApplication("","Auto Zip Saves",      (DWORD)true));
	AddHandler(AutoSleep,        new CSettingTypeApplication("","Auto Sleep",          (DWORD)true));
	AddHandler(AutoFullScreen,   new CSettingTypeApplication("","Auto Full Screen",    (DWORD)true));
	AddHandler(BasicMode,        new CSettingTypeApplication("","Basic Mode",          (DWORD)true));
	AddHandler(RememberCheats,   new CSettingTypeApplication("","Remember Cheats",     (DWORD)false));
	AddHandler(DisableScrSaver,  new CSettingTypeApplication("","Disable Screen Saver",(DWORD)true));
	AddHandler(ShowCPUPer,       new CSettingTypeApplication("","Display CPU Usage",   (DWORD)false));
	AddHandler(LimitFPS,         new CSettingTypeApplication("","Limit FPS",           (DWORD)true));
	AddHandler(ProfileCode,      new CSettingTypeApplication("","Profile Code",        (DWORD)false));
	AddHandler(GenerateLogFiles, new CSettingTypeApplication("","Generate Log Files",  (DWORD)false));
	AddHandler(AlwaysOnTop,      new CSettingTypeApplication("","Always on top",       (DWORD)false));
	AddHandler(DisableGameFixes, new CSettingTypeApplication("","Disable Game Fixes",  (DWORD)false));
	AddHandler(IsValidExe,       new CSettingTypeTempBool(true));

	AddHandler(DisplayFrameRate, new CSettingTypeApplication("","Display Frame Rate",  (DWORD)true));
	AddHandler(FrameDisplayType, new CSettingTypeApplication("","Frame Rate Display Type", (DWORD)FR_VIs));

	//Logging
	AddHandler(AppLogLevel,new CSettingTypeApplication("Logging","Log Level",(DWORD)TraceError));
	AddHandler(AppLogFlush,new CSettingTypeApplication("Logging","Log Auto Flush",(DWORD)false));

	//Recent Files
	AddHandler(RememberedRomFilesCount, new CSettingTypeApplication("","Remembered Rom Files",(DWORD)10));
	AddHandler(RecentRomFileIndex,      new CSettingTypeApplicationIndex("Recent File","Recent Rom",No_Default));
	AddHandler(RememberedRomDirCount,   new CSettingTypeApplication("","Remembered Rom Dirs",(DWORD)10));
	AddHandler(RecentRomDirIndex,      new CSettingTypeApplicationIndex("Recent Dir","Recent Dir",No_Default));

/*	for (count = FirstRecentRom; count != LastRecentRom; count++ ) {
		char Name[300];
		_snprintf(Name,sizeof(Name)," %d",count - FirstRecentRom);
		INF2((SettingID)count,No_Default,Data_String,SettingLoc,Name,"",0,FirstRecentRom);
	}

	//Recent Dirs
	for (count = FirstRecentDir; count != LastRecentDir; count++ ) {
		char Name[300];
		_snprintf(Name,sizeof(Name),"Recent Dir %d",count - FirstRecentDir);
		INF2((SettingID)count,No_Default,Data_String,SettingLoc,Name,"Recent Dir",0,FirstRecentDir);
	}
	INFO(RememberedRomDir,Default_RememberedRomDirs,Data_DWORD,SettingLoc,"Remembered Rom Dirs","",0);
	

	//Plugins
*/
	AddHandler(CurrentRSP_Plugin,   new CSettingTypeApplication("Plugin","RSP Dll",       "RSP\\RSP 1.7.dll"));
	AddHandler(CurrentGFX_Plugin,   new CSettingTypeApplication("Plugin","Graphics Dll",  "GFX\\Jabo_Direct3D8.dll"));
	AddHandler(CurrentAUDIO_Plugin, new CSettingTypeApplication("Plugin","Audio Dll",     "Audio\\Jabo_Dsound.dll"));
	AddHandler(CurrentCONT_Plugin,  new CSettingTypeApplication("Plugin","Controller Dll","Input\\Jabo_DInput.dll"));

	AddHandler(CurVerRSP_Plugin,    new CSettingTypeApplication("Plugin","RSP Dll Ver",        ""));
	AddHandler(CurVerGFX_Plugin,    new CSettingTypeApplication("Plugin","Graphics Dll Ver",   ""));
	AddHandler(CurVerAUDIO_Plugin,  new CSettingTypeApplication("Plugin","Audio Dll Ver",      ""));
	AddHandler(CurVerCONT_Plugin,   new CSettingTypeApplication("Plugin","Controller Dll Ver", ""));

	/*INFO(RSP_PluginChanged,  No_Default,Data_DWORD,TemporarySetting,"","",0);
	INFO(AUDIO_PluginChanged,No_Default,Data_DWORD,TemporarySetting,"","",0);
	INFO(GFX_PluginChanged,  No_Default,Data_DWORD,TemporarySetting,"","",0);
	INFO(CONT_PluginChanged, No_Default,Data_DWORD,TemporarySetting,"","",0);

	//Cheats
	for (count = 0; count < MaxCheats; count++ ) {
		char Name[300];
		
		_snprintf(Name,sizeof(Name),"Cheat%d",count);
		INF2((SettingID)(count + CheatEntry),    No_Default,Data_String,CheatSetting,Name,"",0,CheatEntry);

		_snprintf(Name,sizeof(Name),"Cheat%d",count);
		INF2((SettingID)(count + CheatPermEntry),No_Default,Data_String,RomSetting,Name,"",0,CheatEntry);
		
		_snprintf(Name,sizeof(Name),"Cheat%d_O",count);
		INF2((SettingID)(count + CheatOptions),  No_Default,Data_String,CheatSetting,Name,"",0,CheatOptions);
		
		_snprintf(Name,sizeof(Name),"Cheat%d_R",count);
		INF2((SettingID)(count + CheatRange),  No_Default,Data_String,CheatSetting,Name,"",0,CheatRange);
		
		_snprintf(Name,sizeof(Name),"Cheat%d_RN",count);
		INF2((SettingID)(count + CheatRangeNotes),  No_Default,Data_String,CheatSetting,Name,"",0,CheatRangeNotes);

		_snprintf(Name,sizeof(Name),"Cheat%d_N",count);
		INF2((SettingID)(count + CheatNotes),  No_Default,Data_String,CheatSetting,Name,"",0,CheatNotes);

		_snprintf(Name,sizeof(Name),"Cheat%d",count);
		INF2((SettingID)(count + CheatActive),   Default_False,Data_DWORD,GameSetting,Name,"",0,CheatActive);
		
		_snprintf(Name,sizeof(Name),"Cheat%d.exten",count);
		INF2((SettingID)(count + CheatExtension),Default_CheatExt,Data_String,GameSetting,Name,"",0,CheatExtension);
	}
	
	INFO(RememberedRomDir,Default_RememberedRomDirs,Data_DWORD,SettingLoc,"Remembered Rom Dirs","",0);
*/

	//Directories
	AddHandler(InitialPluginDirectory,      new CSettingTypeRelativePath("Plugin",""));
	AddHandler(InitialRomDirectory,         new CSettingTypeRelativePath("Rom Directory",""));
	AddHandler(InitialSaveDirectory,        new CSettingTypeRelativePath("Save",""));
	AddHandler(InitialInstantSaveDirectory, new CSettingTypeRelativePath("Save",""));
	AddHandler(InitialSnapShotDir,          new CSettingTypeRelativePath("Screenshots",""));
	AddHandler(InitialTextureDir,           new CSettingTypeRelativePath("textures-load",""));

	AddHandler(SelectedPluginDirectory,      new CSettingTypeApplication("Directory","Selected Plugin Dir",       InitialPluginDirectory));
	AddHandler(SelectedRomDir,               new CSettingTypeApplication("Directory","Selected Rom Dir",          InitialRomDirectory));
	AddHandler(SelectedSaveDirectory,        new CSettingTypeApplication("Directory","Selected Save Dir",         InitialSaveDirectory));
	AddHandler(SelectedInstantSaveDirectory, new CSettingTypeApplication("Directory","Selected Instant Save Dir", InitialInstantSaveDirectory));
	AddHandler(SelectedSnapShotDir,          new CSettingTypeApplication("Directory","Selected Snap Shot Dir",    InitialSnapShotDir));
	
	AddHandler(PluginDirectory,      new CSettingTypeApplication("Directory","Plugin Directory",       InitialPluginDirectory));
	AddHandler(RomDirectory,         new CSettingTypeApplication("Directory","Rom Directory",          InitialRomDirectory));
	AddHandler(SaveDirectory,        new CSettingTypeApplication("Directory","Save Directory",         InitialSaveDirectory));
	AddHandler(InstantSaveDirectory, new CSettingTypeApplication("Directory","Instant Save  Directory",InitialInstantSaveDirectory));
	AddHandler(SnapShotDir,          new CSettingTypeApplication("Directory","Snap Shot Directory",    InitialSnapShotDir));
	AddHandler(TextureDir,           new CSettingTypeApplication("Directory","Texture Directory",      InitialTextureDir));
	AddHandler(LastSaveDir,          new CSettingTypeApplication("Directory","Last Save Directory",    InstantSaveDirectory));


	AddHandler(UsePluginDirSelected,   new CSettingTypeApplication("Directory","Use Default Plugin Dir",    false));
	AddHandler(UseRomDirSelected,      new CSettingTypeApplication("Directory","Use Default Rom Dir",       InitialPluginDirectory));
	AddHandler(UseSaveDirSelected,     new CSettingTypeApplication("Directory","Use Default Save Dir",      InitialPluginDirectory));
	AddHandler(UseInstantDirSelected,  new CSettingTypeApplication("Directory","Use Default Instant Dir",   InitialPluginDirectory));
	AddHandler(UseSnapShotDirSelected, new CSettingTypeApplication("Directory","Use Default Snap Shot Dir", InitialPluginDirectory));
/*	
	INFO(ApplicationDir,No_Default,Data_String,RelativePath,"","",0);
*/
	//Debugger
	AddHandler(Debugger,               new CSettingTypeApplication("Debugger","Debugger",false));
	AddHandler(ShowUnhandledMemory,    new CSettingTypeApplication("Debugger","Show Unhandled Memory",false));
	AddHandler(ShowPifErrors,          new CSettingTypeApplication("Debugger","Show Pif Errors",false));
	AddHandler(ShowDListAListCount,    new CSettingTypeApplication("Debugger","Show Dlist Alist Count",false));
	AddHandler(ShowRecompMemSize,      new CSettingTypeApplication("Debugger","Show Recompiler Memory size",false));
	AddHandler(ShowPifRamErrors,       new CSettingTypeApplication("Debugger","Show Pif Ram Errors",false));
	AddHandler(ShowCheckOpUsageErrors, new CSettingTypeApplication("Debugger","Show Check Op Usage Errors",false));
	AddHandler(LogFunctionCalls,       new CSettingTypeApplication("Debugger","Log Function Class",false));
	AddHandler(GenerateDebugLog,       new CSettingTypeApplication("Debugger","Generate Debug Code",false));
	
	//RSP
	AddHandler(UseHighLevelAudio,      new CSettingTypeApplication("Plugin Directory","RSP",false));
	AddHandler(UseHighLevelGfx,        new CSettingTypeApplication("Plugin Directory","RSP",true));
	
	//Idvidual Game Settings
/*	INFO(Game_LastSaveSlot,Default_SaveSlot,Data_DWORD,GameSetting,"Last Used Save Slot","",0);

	//Rom Settings
*/
	AddHandler(ROM_IniKey,        new CSettingTypeTempString(""));
	AddHandler(ROM_NAME,          new CSettingTypeTempString(""));
	//AddHandler(ROM_Default,       new CSettingTypeTempNumber(-1));
	AddHandler(ROM_MD5,           new CSettingTypeRomDatabase("MD5",""));
	AddHandler(ROM_InternalName,  new CSettingTypeRomDatabase("Internal Name",""));
	AddHandler(ROM_GoodName,      new CSettingTypeRomDatabase("Good Name",ROM_NAME));
	AddHandler(ROM_Status,        new CSettingTypeRomDatabase("Status","Unknown"));
	AddHandler(ROM_CoreNotes,     new CSettingTypeRomDatabase("Core Note",""));
	AddHandler(ROM_PluginNotes,   new CSettingTypeRomDatabase("Plugin Note",""));

	//INFO(ROM_CPUType,        ROM_Default,       Data_CPUTYPE,RomSetting,"CPU Type","",0);
	AddHandler(ROM_RomInMemory,   new CSettingTypeRomDatabase("Rom In Memory",false));
	AddHandler(ROM_FunctionLookup,new CSettingTypeRomDatabase("FuncFind",-1));
	AddHandler(ROM_RamSize,       new CSettingTypeRomDatabase("RDRAM Size",-1));
	//INFO(ROM_SaveChip,       Default_SaveChip,  Data_SaveChip,RomSetting,"Save Type","",0);
	AddHandler(ROM_CounterFactor, new CSettingTypeRomDatabase("Counter Factor",-1));
	AddHandler(ROM_CustomSMM,     new CSettingTypeRomDatabase("CustomSMM",false));
	AddHandler(ROM_SMM_Cache,     new CSettingTypeRomDatabase("SMM-Cache",true));
	AddHandler(ROM_SMM_PIDMA,     new CSettingTypeRomDatabase("SMM-PI DMA",true));
	AddHandler(ROM_SMM_TLB,       new CSettingTypeRomDatabase("SMM-TLB",true));
	AddHandler(ROM_SMM_Protect,   new CSettingTypeRomDatabase("SMM-Protect",false));
	AddHandler(ROM_SMM_ValidFunc, new CSettingTypeRomDatabase("SMM-FUNC",true));
	//INFO(ROM_UseTlb,         Default_True,      Data_YesNo,  RomSetting,"Use TLB","",0);
	//INFO(ROM_RegCache,       Default_True,      Data_YesNo,  RomSetting,"Reg Cache","",0);
	AddHandler(ROM_SyncAudio,     new CSettingTypeRomDatabase("Sync Audio",false));
	//INFO(ROM_BlockLinking,   ROM_Default,       Data_OnOff,  RomSetting,"Linking","",0);
	AddHandler(ROM_DelayDlists,     new CSettingTypeRomDatabase("Delay Dlist",true));
	AddHandler(ROM_UseJumpTable,    new CSettingTypeRomDatabase("Use Jump Table",true));
	//INFO(ROM_DelaySI,        Default_False,     Data_YesNo,  RomSetting,"Delay SI","",0);
	//INFO(ROM_AudioSignal,    Default_False,     Data_YesNo,  RomSetting,"Audio Signal","",0);
	//INFO(ROM_SPHack,         Default_False,     Data_YesNo,  RomSetting,"SP Hack","",0);
	AddHandler(ROM_FixedAudio,    new CSettingTypeRomDatabase("Fixed Audio",true));
	AddHandler(ROM_TLB_VAddrStart,new CSettingTypeRomDatabase("TLB: Vaddr Start",0));
	AddHandler(ROM_TLB_VAddrLen,  new CSettingTypeRomDatabase("TLB: Vaddr Len",0));
	AddHandler(ROM_TLB_PAddrStart,new CSettingTypeRomDatabase("TLB: PAddr Start",0));
	
/*	for (count = ROM_MD5 + 1; count != ROM_LastMD5; count++ ) {
		char Name[300];
		_snprintf(Name,sizeof(Name),"MD5%d",count - ROM_LastMD5);
		INFO((SettingID)count,No_Default,Data_String,RomSetting,Name,"",0);
	}

	//System Settings
	INFO(SYSTEM_CPUType,       Default_CPUType,        Data_DWORD,  SettingLoc,"CPU Type","CPU",0);
	INFO(SYSTEM_SelfModMethod, Default_SelfModCheck,   Data_DWORD,  SettingLoc,"Self Mod Method","CPU",0);
	INFO(SYSTEM_FunctionLookup,Default_FunctionLookup, Data_DWORD,  SettingLoc,"Function Lookup","CPU",0);
	INFO(SYSTEM_RDRamSize,     Default_RdramSize,      Data_DWORD,  SettingLoc,"RDRAM Size","CPU",0);
	INFO(SYSTEM_BlockLinking,  Default_BlockLinking,   Data_DWORD,  SettingLoc,"Advanced Block Linking","CPU",0);
	INFO(SYSTEM_SMM_Cache,     Default_True,           Data_DWORD,  SettingLoc,"SMM-Cache","",0);
	INFO(SYSTEM_SMM_PIDMA,     Default_True,           Data_DWORD,  SettingLoc,"SMM-DMA","",0);
	INFO(SYSTEM_SMM_TLB,       Default_True,           Data_DWORD,  SettingLoc,"SMM-TLB","",0);
	INFO(SYSTEM_SMM_ValidFunc, Default_True,          Data_DWORD,  SettingLoc,"SMM-Validate","",0);
	INFO(SYSTEM_SMM_Protect,   Default_False,          Data_DWORD,  SettingLoc,"SMM-Protect","",0);
*/
	// Verifier
	AddHandler(BetaUserNameMD5,        new CSettingTypeTempString("CBBABA8D2262FF1F7A47CEAD87FC4304"));
	AddHandler(BetaEmailAddressMD5,    new CSettingTypeTempString("47A3D7CBF1DA291D5EB30DCAAF21B9F8"));

	//Currrent Running Information
	AddHandler(CPU_Paused,      new CSettingTypeTempBool(false));
	AddHandler(CPU_Paused_type, new CSettingTypeTempNumber(No_Default));
/*	INFO(RamSize,         No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	INFO(CPUType,         No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	INFO(BlockLinking,    No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	INFO(DelayDlists,     No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	INFO(DelaySI,         No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	INFO(CounterFactor,   No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	INFO(UseJumpTable,    No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	INFO(RomInMemory,     No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	INFO(SyncViaAudio,    No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	INFO(UseTLB,          No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	INFO(AudioSignal,     No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	INFO(FuncLookupMode,  No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	INFO(ApplicationName, No_Default,        Data_String, TemporarySetting,"","",0);
	INFO(SaveChipType,    No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	INFO(FirstDMA,        No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	INFO(ShowPifErrors,   Default_True,      Data_DWORD,  TemporarySetting,"","",0);
	INFO(CurrentSaveState,No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	INFO(InstantSaveFile, No_Default,        Data_String, TemporarySetting,"","",0);
*/
	AddHandler(CurrentSaveState, new CSettingTypeTempNumber(0));
	AddHandler(LoadingRom,       new CSettingTypeTempBool(false));
	AddHandler(CPU_Running,      new CSettingTypeTempBool(false));
	AddHandler(ScreenHertz,      new CSettingTypeTempNumber(60));
	AddHandler(InFullScreen,     new CSettingTypeTempBool(false));
/*	INFO(InFullScreen,    Default_False,     Data_DWORD,  TemporarySetting,"","",0);
	INFO(SMM_Cache,       No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	INFO(SMM_PIDMA,       No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	INFO(SMM_TLB,         No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	INFO(SMM_Protect,     No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	INFO(SMM_ValidFunc,   No_Default,        Data_DWORD,  TemporarySetting,"","",0);
	
#undef INFO
	  */
}


DWORD CSettings::GetSetting ( CSettings * _this, SettingID Type )
{
	return _this->LoadDword(Type);
}

const char * CSettings::GetSettingSz ( CSettings * _this, SettingID Type, char * Buffer, int BufferSize )
{
	Notify().BreakPoint(__FILE__,__LINE__); 
/*	if (Buffer && BufferSize > 0)
	{
		Buffer[0] = 0;
		_this->Load(Type, Buffer, BufferSize);
	}*/
	return Buffer;
}

void CSettings::SetSetting ( CSettings * _this, SettingID ID, unsigned int Value )
{
	Notify().BreakPoint(__FILE__,__LINE__); 
//	_this->SaveDword(ID,Value);
}

void CSettings::SetSettingSz ( CSettings * _this, SettingID ID, const char * Value )
{
	Notify().BreakPoint(__FILE__,__LINE__); 
//	_this->SaveString(ID,Value);
}

void CSettings::RegisterSetting ( CSettings * _this, SettingID ID, SettingID DefaultID, SettingDataType Type, 
                                      SettingLocation Location, const char * Category, const char * DefaultStr, 
									  DWORD Value )
{
	switch (Location)
	{
	case SettingLocation_ConstValue:
		if (Type != Data_DWORD) 
		{
			Notify().BreakPoint(__FILE__,__LINE__); 
			return;
		}
		_this->AddHandler(ID,new CSettingTypeTempNumber(Value));
		break;
	case SettingLocation_ConstString:
		if (Type != Data_String) 
		{
			Notify().BreakPoint(__FILE__,__LINE__); 
			return;
		}
		_this->AddHandler(ID,new CSettingTypeTempString(DefaultStr));
		break;
	case SettingLocation_CfgFile:
	case SettingLocation_Registry:
		switch (Type)
		{
		case Data_DWORD:
			if (DefaultID == No_Default)
			{
				_this->AddHandler(ID,new CSettingTypeApplication(Category,DefaultStr,Value));
			} else {
				_this->AddHandler(ID,new CSettingTypeApplication(Category,DefaultStr,DefaultID));
			}
			break;
		case Data_String:
			if (DefaultID == No_Default)
			{
				_this->AddHandler(ID,new CSettingTypeApplication(Category,DefaultStr,""));
			} else {
				_this->AddHandler(ID,new CSettingTypeApplication(Category,DefaultStr,DefaultID));
			}
			break;
		default:
			Notify().BreakPoint(__FILE__,__LINE__); 
		}
		break;
	case SettingLocation_GameSetting:
		switch (Type)
		{
		case Data_DWORD:
			if (DefaultID == No_Default)
			{
				_this->AddHandler(ID,new CSettingTypeGame(Category,DefaultStr,Value));
			} else {
				_this->AddHandler(ID,new CSettingTypeGame(Category,DefaultStr,DefaultID));
			}
			break;
		default:
			Notify().BreakPoint(__FILE__,__LINE__); 
		}
		break;
	default:
		Notify().BreakPoint(__FILE__,__LINE__); 
	}
}

/*#include <windows.h>

*/

/*CSettings::CSettings() :
	RomIniFile(NULL),
	CheatIniFile(NULL),
	SettingsIniFile(NULL),
	UnknownSetting_RSP(NULL),
	UnknownSetting_GFX(NULL),
	UnknownSetting_AUDIO(NULL),
	UnknownSetting_CTRL(NULL)
{
}

*/

bool CSettings::Initilize( const char * AppName )
{
	AddHowToHandleSetting();
	CSettingTypeApplication::Initilize(AppName);
	CSettingTypeRomDatabase::Initilize();

/*	strncpy(Registrylocation,"Software\\N64 Emulation\\",sizeof(Registrylocation));
	strncat(Registrylocation,AppName,sizeof(Registrylocation));
	RegistryKey = (int)HKEY_CURRENT_USER;

	if (SettingInfo.size() == 0) {
		AddHowToHandleSetting();
	}
	
	_Settings->SaveString(ApplicationName,AppName);

	RomIniFile   = new CIniFile(LoadString(IniName).c_str());
	CheatIniFile = new CIniFile(LoadString(CheatIniName).c_str());
*/
	return true;
}

bool CSettings::LoadBool ( SettingID Type )
{
	SETTING_HANDLER FindInfo = m_SettingInfo.find(Type);
	if (FindInfo == m_SettingInfo.end()) 
	{  
		//if not found do nothing
		UnknownSetting(Type);
		return 0; 
	}
	bool Value = false;
	if (FindInfo->second->IndexBasedSetting())
	{
		Notify().BreakPoint(__FILE__,__LINE__); 
	} else {
		FindInfo->second->Load(0,Value);
	}
	return Value;
}

bool CSettings::LoadBool ( SettingID Type, bool & Value )
{
	Notify().BreakPoint(__FILE__,__LINE__); 
	return false;
}

bool CSettings::LoadBoolIndex( SettingID Type, int index )
{
	Notify().BreakPoint(__FILE__,__LINE__); 
	return false;
}

bool CSettings::LoadBoolIndex( SettingID Type, int index , bool & Value )
{
	Notify().BreakPoint(__FILE__,__LINE__); 
	return false;
}

DWORD CSettings::LoadDword ( SettingID Type )
{
	SETTING_HANDLER FindInfo = m_SettingInfo.find(Type);
	if (FindInfo == m_SettingInfo.end()) 
	{  
		//if not found do nothing
		UnknownSetting(Type);
		return 0; 
	}
	DWORD Value = 0;
	if (FindInfo->second->IndexBasedSetting())
	{
		Notify().BreakPoint(__FILE__,__LINE__); 
	} else {
		FindInfo->second->Load(0,Value);
	}
	return Value;
}

bool CSettings::LoadDword ( SettingID Type, DWORD & Value)
{
	SETTING_HANDLER FindInfo = m_SettingInfo.find(Type);
	if (FindInfo == m_SettingInfo.end()) 
	{  
		//if not found do nothing
		UnknownSetting(Type);
		return 0; 
	}
	if (FindInfo->second->IndexBasedSetting())
	{
		Notify().BreakPoint(__FILE__,__LINE__); 
	} else {
		DWORD TempValue = 0;
		if (FindInfo->second->Load(0,TempValue))
		{
			Value = TempValue;
			return true;
		}
	}
	return false;
}

DWORD CSettings::LoadDwordIndex( SettingID Type, int index)
{
	SETTING_HANDLER FindInfo = m_SettingInfo.find(Type);
	if (FindInfo == m_SettingInfo.end()) 
	{  
		//if not found do nothing
		UnknownSetting(Type);
		return 0; 
	}
	DWORD Value = 0;
	if (FindInfo->second->IndexBasedSetting())
	{
		FindInfo->second->Load(index,Value);
	} else {
		Notify().BreakPoint(__FILE__,__LINE__); 
	}
	return Value;
}

bool CSettings::LoadDwordIndex( SettingID Type, int index, DWORD & Value)
{
	SETTING_HANDLER FindInfo = m_SettingInfo.find(Type);
	if (FindInfo == m_SettingInfo.end()) 
	{  
		//if not found do nothing
		UnknownSetting(Type);
		return 0; 
	}
	if (FindInfo->second->IndexBasedSetting())
	{
		DWORD TempValue = 0;
		if (FindInfo->second->Load(index,TempValue))
		{
			Value = TempValue;
			return true;
		}
	} else {
		Notify().BreakPoint(__FILE__,__LINE__); 
	}
	return false;
}

stdstr CSettings::LoadString ( SettingID Type )
{
	SETTING_HANDLER FindInfo = m_SettingInfo.find(Type);
	if (FindInfo == m_SettingInfo.end()) 
	{  
		//if not found do nothing
		UnknownSetting(Type);
		return 0; 
	}
	stdstr Value;
	if (FindInfo->second->IndexBasedSetting())
	{
		Notify().BreakPoint(__FILE__,__LINE__); 
	} else {
		FindInfo->second->Load(0,Value);
	}
	return Value;
}

bool CSettings::LoadString ( SettingID Type, stdstr & Value )
{
	SETTING_HANDLER FindInfo = m_SettingInfo.find(Type);
	if (FindInfo == m_SettingInfo.end()) 
	{  
		//if not found do nothing
		UnknownSetting(Type);
		return 0; 
	}
	if (FindInfo->second->IndexBasedSetting())
	{
		Notify().BreakPoint(__FILE__,__LINE__); 
	} else {
		return FindInfo->second->Load(0,Value);
	}
	return false;
}

bool CSettings::LoadString ( SettingID Type, char * Buffer, int BufferSize )
{
	Notify().BreakPoint(__FILE__,__LINE__); 
	return false;
}

stdstr CSettings::LoadStringIndex ( SettingID Type, int index )
{
	SETTING_HANDLER FindInfo = m_SettingInfo.find(Type);
	if (FindInfo == m_SettingInfo.end()) 
	{  
		//if not found do nothing
		UnknownSetting(Type);
		return 0; 
	}
	stdstr Value;
	if (FindInfo->second->IndexBasedSetting())
	{
		FindInfo->second->Load(index,Value);
	} else {
		Notify().BreakPoint(__FILE__,__LINE__); 
	}
	return Value;
}

bool CSettings::LoadStringIndex ( SettingID Type, int index, stdstr & Value )
{
	Notify().BreakPoint(__FILE__,__LINE__); 
	return false;
}

bool CSettings::LoadStringIndex ( SettingID Type, int index, char * Buffer, int BufferSize )
{
	Notify().BreakPoint(__FILE__,__LINE__); 
	return false;
}

void CSettings::SaveBool ( SettingID Type, bool Value )
{
	SETTING_HANDLER FindInfo = m_SettingInfo.find(Type);
	if (FindInfo == m_SettingInfo.end()) 
	{  
		//if not found do nothing
		UnknownSetting(Type);
		return;
	}
	if (FindInfo->second->IndexBasedSetting())
	{
		Notify().BreakPoint(__FILE__,__LINE__); 
	} else {
		FindInfo->second->Save(0,Value);
	}
	NotifyCallBacks(Type);
}

void CSettings::SaveBoolIndex( SettingID Type, int index, bool Value )
{
	Notify().BreakPoint(__FILE__,__LINE__); 
}

void CSettings::SaveDword ( SettingID Type, DWORD Value )
{
	SETTING_HANDLER FindInfo = m_SettingInfo.find(Type);
	if (FindInfo == m_SettingInfo.end()) 
	{  
		//if not found do nothing
		UnknownSetting(Type);
		return;
	}
	if (FindInfo->second->IndexBasedSetting())
	{
		Notify().BreakPoint(__FILE__,__LINE__); 
	} else {
		FindInfo->second->Save(0,Value);
	}
	NotifyCallBacks(Type);
}

void CSettings::SaveDwordIndex ( SettingID Type, int index, DWORD Value )
{
	SETTING_HANDLER FindInfo = m_SettingInfo.find(Type);
	if (FindInfo == m_SettingInfo.end()) 
	{  
		//if not found do nothing
		UnknownSetting(Type);
		return;
	}
	if (FindInfo->second->IndexBasedSetting())
	{
		FindInfo->second->Save(index,Value);
	} else {
		Notify().BreakPoint(__FILE__,__LINE__); 
	}
	NotifyCallBacks(Type);
}

void CSettings::SaveString ( SettingID Type, const stdstr & value )
{
	Notify().BreakPoint(__FILE__,__LINE__); 
}

void CSettings::SaveString ( SettingID Type, const char * Buffer )
{
	SETTING_HANDLER FindInfo = m_SettingInfo.find(Type);
	if (FindInfo == m_SettingInfo.end()) 
	{  
		//if not found do nothing
		UnknownSetting(Type);
	}
	if (FindInfo->second->IndexBasedSetting())
	{
		Notify().BreakPoint(__FILE__,__LINE__); 
	} else {
		FindInfo->second->Save(0,Buffer);
	}
	NotifyCallBacks(Type);
}

void CSettings::SaveStringIndex( SettingID Type, int index, const char * Buffer )
{
	Notify().BreakPoint(__FILE__,__LINE__); 
}

void CSettings::SaveStringIndex( SettingID Type, int index, const stdstr & Value )
{
	SETTING_HANDLER FindInfo = m_SettingInfo.find(Type);
	if (FindInfo == m_SettingInfo.end()) 
	{  
		//if not found do nothing
		UnknownSetting(Type);
	}
	if (FindInfo->second->IndexBasedSetting())
	{
		FindInfo->second->Save(index,Value);
	} else {
		Notify().BreakPoint(__FILE__,__LINE__); 
	}
	NotifyCallBacks(Type);
}

void CSettings::UnknownSetting (SettingID Type)
{
#ifdef _DEBUG
	Notify().BreakPoint(__FILE__,__LINE__); 
#endif
}

/*CSettings::~CSettings() {
	if (RomIniFile) {
		delete RomIniFile;
		RomIniFile = NULL;
	}
	if (CheatIniFile) {
		delete CheatIniFile;
		CheatIniFile = NULL;
	}
	if (SettingsIniFile)
	{
		delete SettingsIniFile;
		SettingsIniFile = NULL;
	}
}

void CSettings::Load(SettingID Type, char * Buffer, int BufferSize) {
	if (Type != IsValidExe)
	{
		WriteTraceF(TraceSettings,"CSettings::Load 1 - %d",Type);
	}

	//Find out the information for handling the setting type from the list
	SETTING_MAP::iterator FindInfo = SettingInfo.find(Type);
	if (FindInfo == SettingInfo.end()) 
	{  //if not found do nothing
		UnknownSetting(Type);
		return; 
	}
	CSettingInfo * Info = &FindInfo->second;
			
	if (Info->DefaultValue != No_Default) {
		Load(Info->DefaultValue,Buffer, BufferSize);
	}
	//Copy Default String
	
	if (Info->Location == ConstString) {
		strncpy(Buffer,Info->Name.c_str(),BufferSize);
	} else if (Info->Location == RelativePath) {
		char path_buffer[_MAX_PATH], drive[_MAX_DRIVE] ,dir[_MAX_DIR];
		char fname[_MAX_FNAME],ext[_MAX_EXT];

		GetModuleFileName(NULL,path_buffer,sizeof(path_buffer));
		_splitpath( path_buffer, drive, dir, fname, ext );
		sprintf(path_buffer,"%s%s",drive,dir);
		if (Info->SubNode.size() > 0) {
			strncat(path_buffer,Info->SubNode.c_str(),sizeof(path_buffer));
			strncat(path_buffer,"\\",sizeof(path_buffer));
		}
		if (Info->Name.size() > 0) {
			strncat(path_buffer,Info->Name.c_str(),sizeof(path_buffer));
		}
		strncpy(Buffer,path_buffer,BufferSize);
	} else if (Info->Location == TemporarySetting) {
		CGuard Guard(m_CS);
	
		TEMP_SETTING_MAP::iterator TempInfo = TempKeys.find(Type);
		if (TempInfo != TempKeys.end()) {
			CTempInfo * temp = &TempInfo->second;
			if (temp->DataType == Data_String) {
				strncpy(Buffer,temp->String,BufferSize);
			}
		}
	} else if (Info->Location == LocalSettings) {
		if (SettingsIniFile == NULL) { return; }
		if (Info->DataType == Data_String) 
		{
			stdstr_f Ident("%s",Info->SubNode.c_str());
			if (Ident.empty())
			{
				Ident = "default";
			}
			Ident.replace("\\","-");
			SettingsIniFile->GetString(Ident.c_str(),Info->Name.c_str(),Buffer,Buffer,BufferSize);
		} else {
#ifdef _DEBUG
			Notify().BreakPoint(__FILE__,__LINE__); 
#endif
		}
	} else if (Info->Location == InRegistry) {
		char String[255];

		//Get the location in the registry we are writing to
		strncpy(String,Registrylocation,sizeof(String));
		if (Info->SubNode.size() > 0) {
			strncat(String,"\\",sizeof(String));
			strncat(String,Info->SubNode.c_str(),sizeof(String));
		}
		
		//Open the registry key
		long lResult;
		HKEY hKeyResults = 0;
		lResult = RegOpenKeyEx( (HKEY)RegistryKey,String,0, KEY_ALL_ACCESS,&hKeyResults);

		if (lResult == ERROR_SUCCESS) {
			//Get the value from the registry
			DWORD RegType, Bytes = BufferSize;
			
			lResult = RegQueryValueEx(hKeyResults,Info->Name.c_str(),0,&RegType,(LPBYTE)(Buffer),&Bytes);
			if (RegType != REG_SZ || lResult != ERROR_SUCCESS) { 
				//Reload Defaults just in case data has changed
				if (Info->DefaultValue != No_Default) {
					Load(Info->DefaultValue,Buffer, BufferSize);
				}
			}
			RegCloseKey(hKeyResults);
		}
	} else if (Info->Location == RomSetting) {
		if (RomIniFile == NULL) { return; }
		if (Info->DataType == Data_String) {
			RomIniFile->GetString(LoadString(ROM_IniKey).c_str(),Info->Name.c_str(),Buffer,Buffer,BufferSize);
		}
	} else if (Info->Location == CheatSetting) {
		if (CheatIniFile == NULL) { return; }
		if (Info->DataType == Data_String) {
			CheatIniFile->GetString(LoadString(ROM_IniKey).c_str(),Info->Name.c_str(),Buffer,Buffer,BufferSize);
		}
	} else if (Info->Location == GameSetting) {
		if (SettingsIniFile == NULL) { return; }
		if (Info->DataType == Data_String) 
		{
			char Ident[400];
			//Get the location in the registry we are writing to
			sprintf(Ident,"%s%s%s",LoadString(ROM_IniKey).c_str(),Info->SubNode.empty()?"":"-",Info->SubNode.c_str());

			int len = strlen(Ident);
			for (int i = 0; i < len; i++)
			{
				if (Ident[i] == '\\')
				{
					Ident[i] = '-';
				}
			}
			SettingsIniFile->GetString(Ident,Info->Name.c_str(),Buffer,Buffer,BufferSize);
		} else {
#ifdef _DEBUG
			Notify().BreakPoint(__FILE__,__LINE__); 
#endif
		}
	} else {
		//unknown ??
#ifdef _DEBUG
		Notify().BreakPoint(__FILE__,__LINE__); 
#endif
	}
}

DWORD CSettings::LoadDword(SettingID Type ) {	
	if (Type != IsValidExe)
	{
		WriteTraceF(TraceSettings,"CSettings::Load 2 - %d",Type);
	}
	DWORD Value = 0;
	Load(Type,Value);
	return Value;
}

stdstr CSettings::LoadString ( SettingID Type ) {
	if (Type != IsValidExe)
	{
		WriteTraceF(TraceSettings,"CSettings::Load 3 - %d",Type);
	}

	char Buffer[8000] = "";
	Load(Type,Buffer,sizeof(Buffer));
	return stdstr_f("%s",Buffer);
}

void CSettings::UnknownSetting (SettingID Type)
{
	if (UnknownSetting_RSP)
	{
		if (Type >= FirstRSPDefaultSet && Type < LastRSPDefaultSet)
		{
			UnknownSetting_RSP(Type - FirstRSPDefaultSet);
			return;
		}
		if (Type >= FirstRSPSettings && Type < LastRSPSettings)
		{
			UnknownSetting_RSP(Type - FirstRSPSettings);
			return;
		}
	}
	if (UnknownSetting_GFX)
	{
		if (Type >= FirstGfxDefaultSet && Type < LastGfxDefaultSet)
		{
			UnknownSetting_GFX(Type - FirstGfxDefaultSet);
			return;
		}
		if (Type >= FirstGfxSettings && Type < LastGfxSettings)
		{
			UnknownSetting_GFX(Type - FirstGfxSettings);
			return;
		}
	}
	if (UnknownSetting_AUDIO)
	{
		if (Type >= FirstAudioDefaultSet && Type < LastAudioDefaultSet)
		{
			UnknownSetting_AUDIO(Type - FirstAudioDefaultSet);
			return;
		}
		if (Type >= FirstAudioSettings && Type < LastAudioSettings)
		{
			UnknownSetting_AUDIO(Type - FirstAudioSettings);
			return;
		}
	}
	if (UnknownSetting_CTRL)
	{
		if (Type >= FirstCtrlDefaultSet && Type < LastCtrlDefaultSet)
		{
			UnknownSetting_CTRL(Type - FirstCtrlDefaultSet);
			return;
		}
		if (Type >= FirstCtrlSettings && Type < LastCtrlSettings)
		{
			UnknownSetting_CTRL(Type - FirstCtrlSettings);
			return;
		}
	}
#ifdef _DEBUG
	Notify().BreakPoint(__FILE__,__LINE__); 
#endif
}

void CSettings::Load(SettingID Type, DWORD & Value) 
{
	if (Type == Default_False || Type == Default_True)
	{
		Value = (Type == Default_True);
		return;
	}
	if (Type != IsValidExe)
	{
		WriteTraceF(TraceSettings,"CSettings::Load 4 - %d",Type);
	}

	//Find out the information for handling the setting type from the list
	SETTING_MAP::iterator FindInfo = SettingInfo.find(Type);
	if (FindInfo == SettingInfo.end()) 
	{
		//if not found do nothing
		UnknownSetting(Type);
		return; 
	} 
	CSettingInfo * Info = &FindInfo->second;

	if (Info->DefaultValue != No_Default) {
		Load(Info->DefaultValue,Value);
	}
	if (Info->Location == ConstValue) {
		Value = Info->Value;
	} 
	else if (Info->Location == TemporarySetting) 
	{
		CGuard Guard(m_CS);
	
		TEMP_SETTING_MAP::iterator TempInfo = TempKeys.find(Type);
		if (TempInfo != TempKeys.end()) {
			CTempInfo * temp = &TempInfo->second;
			if (temp->DataType == Data_DWORD) {
				Value = temp->Value;
			}
		}
	} 
	else if (Info->Location == InRegistry)
	{
		char String[255];

		//Get the location in the registry we are writing to
		strncpy(String,Registrylocation,sizeof(String));
		if (Info->SubNode.size() > 0) {
			strncat(String,"\\",sizeof(String));
			strncat(String,Info->SubNode.c_str(),sizeof(String));
		}
		
		//Open the registry key
		long lResult;
		HKEY hKeyResults = 0;
		lResult = RegOpenKeyEx( (HKEY)RegistryKey,String,0, KEY_ALL_ACCESS,&hKeyResults);

		if (lResult == ERROR_SUCCESS) {
			//Get the value from the registry
			DWORD RegValue, RegType, Bytes = 4;
			
			lResult = RegQueryValueEx(hKeyResults,Info->Name.c_str(),0,&RegType,(LPBYTE)(&RegValue),&Bytes);
			if (RegType == REG_DWORD && lResult == ERROR_SUCCESS) { 
				Value = RegValue;
			}
			RegCloseKey(hKeyResults);
		}
	} 
	else if (Info->Location == RomSetting)
	{
		if (RomIniFile == NULL) { return; }
		if (Info->DataType == Data_DWORD) {
			Value = RomIniFile->GetNumber(LoadString(ROM_IniKey).c_str(),Info->Name.c_str(),Value);
		} else if (Info->DataType == Data_CPUTYPE) {
			char String [100];
			strcpy(String,"");
			RomIniFile->GetString(LoadString(ROM_IniKey).c_str(),Info->Name.c_str(),String,String,sizeof(String));
			if (strcmp(String,"Interpreter") == 0)       { Value = CPU_Interpreter; } 
			else if (strcmp(String,"Recompiler") == 0)   { Value = CPU_Recompiler; } 
			else if (strcmp(String,"SyncCores") == 0)    { Value = CPU_SyncCores; } 
			else                                         { Value = CPU_Default; } 
//		} else if (Info->DataType == Data_SelfMod) {
//			char String [100];
//			strcpy(String,"");
//			RomIniFile->GetString(LoadString(ROM_IniKey).c_str(),Info->Name.c_str(),String,String,sizeof(String));
//			if (strcmp(String,"None") == 0)                      { *Value  = ModCode_None; } 
//			else if (strcmp(String,"Cache") == 0)                { *Value  = ModCode_Cache; } 
//			else if (strcmp(String,"Protected Memory") == 0)     { *Value  = ModCode_ProtectedMemory; } 
//			else if (strcmp(String,"Check Memory") == 0)         { *Value  = ModCode_CheckMemoryCache; } 
//			else if (strcmp(String,"Check Memory & cache") == 0) { *Value  = ModCode_CheckMemoryCache; } 
//			else if (strcmp(String,"Check Memory Advance") == 0) { *Value  = ModCode_CheckMemory2; } 
//			else if (strcmp(String,"Change Memory") == 0)        { *Value  = ModCode_ChangeMemory; } 
		} else if (Info->DataType == Data_SaveChip) {
			char String [100];
			strcpy(String,"");
			RomIniFile->GetString(LoadString(ROM_IniKey).c_str(),Info->Name.c_str(),String,String,sizeof(String));
			if (strcmp(String,"4kbit Eeprom") == 0)       { Value = SaveChip_Eeprom_4K; } 
			else if (strcmp(String,"16kbit Eeprom") == 0) { Value = SaveChip_Eeprom_16K; } 
			else if (strcmp(String,"Sram") == 0)          { Value = SaveChip_Sram; } 
			else if (strcmp(String,"FlashRam") == 0)      { Value = SaveChip_FlashRam; } 
		} else if (Info->DataType == Data_OnOff) {
			char String [100];
			strcpy(String,"");
			RomIniFile->GetString(LoadString(ROM_IniKey).c_str(),Info->Name.c_str(),String,String,sizeof(String));
			if (strcmp(String,"On") == 0) { Value = 1; } 
			if (strcmp(String,"Off") == 0) { Value = 0; } 
		} else if (Info->DataType == Data_YesNo) {
			char String [100];
			strcpy(String,"");
			RomIniFile->GetString(LoadString(ROM_IniKey).c_str(),Info->Name.c_str(),String,String,sizeof(String));
			if (strcmp(String,"Yes") == 0) { Value = true; } 
			if (strcmp(String,"No") == 0) { Value = false; } 
		} else {
#ifdef _DEBUG
		Notify().BreakPoint(__FILE__,__LINE__); 
#endif
		}

	}
	else if (Info->Location == GameSetting) 
	{
		char Ident[400];
		//Get the location in the registry we are writing to
		sprintf(Ident,"%s%s%s",LoadString(ROM_IniKey).c_str(),Info->SubNode.empty()?"":"-",Info->SubNode.c_str());

		int len = strlen(Ident);
		for (int i = 0; i < len; i++)
		{
			if (Ident[i] == '\\')
			{
				Ident[i] = '-';
			}
		}

		if (Info->DataType == Data_DWORD) 
		{
			Value = SettingsIniFile->GetNumber(Ident,Info->Name.c_str(),Value);
		}
		else 
		{
#ifdef _DEBUG
		Notify().BreakPoint(__FILE__,__LINE__); 
#endif
		}

	}
	else if (Info->Location == LocalSettings) 
	{
		if (SettingsIniFile == NULL) { return; }
		stdstr_f Ident("%s",Info->SubNode.c_str());
		if (Ident.empty())
		{
			Ident = "default";
		}
		Ident.replace("\\","-");
		if (Info->DataType == Data_DWORD) 
		{
			Value = SettingsIniFile->GetNumber(Ident.c_str(),Info->Name.c_str(),Value);
		}
		else 
		{
#ifdef _DEBUG
		Notify().BreakPoint(__FILE__,__LINE__); 
#endif
		}
	} else {
#ifdef _DEBUG
		Notify().BreakPoint(__FILE__,__LINE__); 
#endif
	}
}

void CSettings::SaveString(SettingID Type, const char * Buffer) {
	//Find out the information for handling the setting type from the list
	SETTING_MAP::iterator FindInfo = SettingInfo.find(Type);
	if (FindInfo == SettingInfo.end()) 
	{  //if not found do nothing
		UnknownSetting(Type);
		return; 
	}
	CSettingInfo * Info = &FindInfo->second;

	//make sure the setting type is a string
	if (Info->DataType != Data_String) 
	{
#ifdef _DEBUG
		Notify().BreakPoint(__FILE__,__LINE__); 
#endif
		return; 
	}

	//create an entry in the temporary settings
	if (Info->Location == TemporarySetting) {
		CGuard Guard(m_CS);
	
		TEMP_SETTING_MAP::iterator TempInfo = TempKeys.find(Type);
		if (TempInfo != TempKeys.end()) { TempKeys.erase(TempInfo); } //free the previous memory

	    TempKeys.insert(TEMP_SETTING_MAP::value_type(Type,CTempInfo(Type,Buffer)));
	}
	
	//Save data to the registry
	else if (Info->Location == InRegistry) {
		char String[255];

		//Get the location in the registry we are writing to
		strncpy(String,Registrylocation,sizeof(String));
		if (Info->SubNode.size() > 0) {
			strncat(String,"\\",sizeof(String));
			strncat(String,Info->SubNode.c_str(),sizeof(String));
		}

		//Create the target Key
		DWORD Disposition = 0;
		HKEY hKeyResults = 0;
		long lResult;

		lResult = RegCreateKeyEx( (HKEY)RegistryKey, String,0,"", REG_OPTION_NON_VOLATILE,
			KEY_ALL_ACCESS,NULL, &hKeyResults,&Disposition);
		if (lResult == ERROR_SUCCESS) {
			RegSetValueEx(hKeyResults,Info->Name.c_str(),0, REG_SZ,(CONST BYTE *)(Buffer),strlen(Buffer));
		}
		RegCloseKey(hKeyResults);
	}
	else if (Info->Location == RomSetting) {
		if (RomIniFile == NULL) { return; }
		RomIniFile->SaveString(LoadString(ROM_IniKey).c_str(),Info->Name.c_str(),Buffer);
	}
	else if (Info->Location == CheatSetting) {
		if (CheatIniFile == NULL) { return; }
		if (Info->DataType == Data_String) {
			stdstr DefaultValue;
			if (Info->DefaultValue != No_Default) {
				DefaultValue = LoadString(Info->DefaultValue);
			}
			if (DefaultValue == Buffer) {
				CheatIniFile->SaveString(LoadString(ROM_IniKey).c_str(),Info->Name.c_str(),NULL);
			} else {
				CheatIniFile->SaveString(LoadString(ROM_IniKey).c_str(),Info->Name.c_str(),Buffer);
			}
		} else {
			Notify().BreakPoint(__FILE__,__LINE__);
		}
	}
	else if (Info->Location == GameSetting) {
		char Ident[400];
		//Get the location in the registry we are writing to
		sprintf(Ident,"%s%s%s",LoadString(ROM_IniKey).c_str(),Info->SubNode.empty()?"":"-",Info->SubNode.c_str());

		int len = strlen(Ident);
		for (int i = 0; i < len; i++)
		{
			if (Ident[i] == '\\')
			{
				Ident[i] = '-';
			}
		}

		stdstr DefaultValue;
		if (Info->DefaultValue != No_Default) {
			DefaultValue = LoadString(Info->DefaultValue);
		}

		if (Info->DataType == Data_String) 
		{
			if (DefaultValue == Buffer) {
				SettingsIniFile->SaveString(Ident,Info->Name.c_str(),NULL);
			} else {
				SettingsIniFile->SaveString(Ident,Info->Name.c_str(),Buffer);
			}
		}
		else 
		{
#ifdef _DEBUG
			Notify().BreakPoint(__FILE__,__LINE__); 
#endif
		}
	} else if (Info->Location == LocalSettings) {
		if (SettingsIniFile == NULL) { return; }
		stdstr DefaultValue;
		if (Info->DefaultValue != No_Default) {
			DefaultValue = LoadString(Info->DefaultValue);
		}
	    stdstr_f Ident("%s",Info->SubNode.c_str());
		Ident.replace("\\","-");
		if (Ident.empty())
		{
			Ident = "default";
		}

		if (DefaultValue == Buffer) {
			SettingsIniFile->SaveString(Ident.c_str(),Info->Name.c_str(),NULL);
		} else {
			SettingsIniFile->SaveString(Ident.c_str(),Info->Name.c_str(),Buffer);
		}
	}
	else {
#ifdef _DEBUG
		Notify().BreakPoint(__FILE__,__LINE__);
#endif
	}
}

void CSettings::SaveDword(SettingID Type, DWORD Value) {
	//Find out the information for handling the setting type from the list
	SETTING_MAP::iterator FindInfo = SettingInfo.find(Type);
	if (FindInfo == SettingInfo.end()) 
	{  //if not found do nothing
		UnknownSetting(Type);
		return; 
	}
	CSettingInfo * Info = &FindInfo->second;

	//create an entry in the temporary settings
	if (Info->Location == TemporarySetting) {
		CGuard Guard(m_CS);
	
		TEMP_SETTING_MAP::iterator TempInfo = TempKeys.find(Type);
		if (TempInfo != TempKeys.end()) { TempKeys.erase(TempInfo); } //free the previous memory
	    TempKeys.insert(TEMP_SETTING_MAP::value_type(Type,CTempInfo(Type,Value)));
	} else if (Info->Location == LocalSettings) {
		if (SettingsIniFile == NULL) { return; }

		DWORD DefaultValue = 0;
		if (Info->DefaultValue != No_Default) {
			Load(Info->DefaultValue,DefaultValue);
		}
		stdstr_f Ident("%s",Info->SubNode.c_str());
		if (Ident.empty())
		{
			Ident = "default";
		}
		Ident.replace("\\","-");
		
		if (Info->DefaultValue != No_Default && Value == DefaultValue) {
			SettingsIniFile->SaveString(Ident.c_str(),Info->Name.c_str(),NULL);
		} else {
			SettingsIniFile->SaveNumber(Ident.c_str(),Info->Name.c_str(),Value);
		}
	} else if (Info->Location == InRegistry) {
		char String[255];

		//Get the location in the registry we are writing to
		strncpy(String,Registrylocation,sizeof(String));
		if (Info->SubNode.size() > 0) {
			strncat(String,"\\",sizeof(String));
			strncat(String,Info->SubNode.c_str(),sizeof(String));
		}

		//Create the target Key
		DWORD Disposition = 0;
		HKEY hKeyResults = 0;
		long lResult;

		lResult = RegCreateKeyEx( (HKEY)RegistryKey, String,0,"", REG_OPTION_NON_VOLATILE,
			KEY_ALL_ACCESS,NULL, &hKeyResults,&Disposition);
		if (lResult == ERROR_SUCCESS) {
			RegSetValueEx(hKeyResults,Info->Name.c_str(),0, REG_DWORD,(CONST BYTE *)(&Value),sizeof(DWORD));
		}
		RegCloseKey(hKeyResults);
	} else if (Info->Location == RomSetting) {
		if (RomIniFile == NULL) { return; }
		if (Info->DataType == Data_DWORD) {
			DWORD DefaultValue = 0;
			if (Info->DefaultValue != No_Default) {
				Load(Info->DefaultValue,DefaultValue);
			}
			if (Info->DefaultValue != No_Default && Value == DefaultValue) {
				RomIniFile->SaveString(LoadString(ROM_IniKey).c_str(),Info->Name.c_str(),NULL);
			} else {
				RomIniFile->SaveNumber(LoadString(ROM_IniKey).c_str(),Info->Name.c_str(),Value);
			}
		} else if (Info->DataType == Data_CPUTYPE) {
			const char * String;
			switch (Value) {
			case CPU_Interpreter: String = "Interpreter"; break;
			case CPU_Recompiler:  String = "Recompiler"; break;
			case CPU_SyncCores:   String = "SyncCores"; break;
			default:              String = NULL; break;
			}
			RomIniFile->SaveString(LoadString(ROM_IniKey).c_str(),Info->Name.c_str(),String);
		} else if (Info->DataType == Data_SaveChip) {
			const char * String;
			switch (Value) {
			case SaveChip_Eeprom_4K: String = "4kbit Eeprom"; break;
			case SaveChip_Eeprom_16K: String = "16kbit Eeprom"; break;
			case SaveChip_Sram: String = "Sram"; break;
			case SaveChip_FlashRam: String = "FlashRam"; break;
			default: String = NULL; break;
			}
			RomIniFile->SaveString(LoadString(ROM_IniKey).c_str(),Info->Name.c_str(),String);
//		} else if (Info->DataType == Data_SelfMod) {
//			const char * String;
//			switch (Value) {
//			case ModCode_None: String = "None"; break;
//			case ModCode_Cache: String = "Cache"; break;
//			case ModCode_ProtectedMemory: String = "Protected Memory"; break;
//			case ModCode_CheckMemoryCache: String = "Check Memory & cache"; break;
//			case ModCode_CheckMemory2: String = "Check Memory Advance"; break;
//			case ModCode_ChangeMemory: String = "Change Memory"; break;
//			default: String = NULL; break;
//			}
//			RomIniFile->SaveString(LoadString(ROM_IniKey).c_str(),Info->Name.c_str(),String);
		} else if (Info->DataType == Data_YesNo) {
			DWORD DefaultValue = 0;
			if (Info->DefaultValue != No_Default) {
				Load(Info->DefaultValue,DefaultValue);
			}
			if (Info->DefaultValue != No_Default && Value == DefaultValue) {
				RomIniFile->SaveString(LoadString(ROM_IniKey).c_str(),Info->Name.c_str(),NULL);
			} else {
				RomIniFile->SaveString(LoadString(ROM_IniKey).c_str(),Info->Name.c_str(),Value ? "Yes" : "No");
			}
		} else if (Info->DataType == Data_OnOff) {
			const char * String;
			switch (Value) {
			case 0: String = "Off"; break;
			case 1: String = "On"; break;
			default: String = NULL; break;
			}
			RomIniFile->SaveString(LoadString(ROM_IniKey).c_str(),Info->Name.c_str(),String);
			
		} else {
#ifdef _DEBUG
			Notify().BreakPoint(__FILE__,__LINE__); 
#endif
		}
	} else if (Info->Location == GameSetting) {
		if (SettingsIniFile == NULL) { return; }
		if (Info->DataType == Data_DWORD) 
		{
			char Ident[400];
			//Get the location in the registry we are writing to
			sprintf(Ident,"%s%s%s",LoadString(ROM_IniKey).c_str(),Info->SubNode.empty()?"":"-",Info->SubNode.c_str());

			int len = strlen(Ident);
			for (int i = 0; i < len; i++)
			{
				if (Ident[i] == '\\')
				{
					Ident[i] = '-';
				}
			}
			
			DWORD DefaultValue = 0;
			if (Info->DefaultValue != No_Default) {
				Load(Info->DefaultValue,DefaultValue);
			}

			if (Info->DefaultValue != No_Default && Value == DefaultValue) {
				SettingsIniFile->SaveString(Ident,Info->Name.c_str(),NULL);
			} else {
				SettingsIniFile->SaveNumber(Ident,Info->Name.c_str(),Value);
			}
		} else{
#ifdef _DEBUG
			Notify().BreakPoint(__FILE__,__LINE__); 
#endif
		}

			
	} else {
#ifdef _DEBUG
		Notify().BreakPoint(__FILE__,__LINE__); 
#endif
	}

	for (SETTING_CHANGED_CB_LIST::iterator iter = m_CBDwordList.begin(); iter != m_CBDwordList.end(); iter ++)
	{
		SETTING_CHANGED_CB & item = *iter;
		if (item.Type != Type) { continue; }
		item.Func(item.Data);
	}

}
*/

void CSettings::NotifyCallBacks( SettingID Type )
{
	SETTING_CALLBACK::iterator Callback = m_Callback.find(Type);
	if (Callback == m_Callback.end())
	{
		return;
	}
		
	for (SETTING_CHANGED_CB * item = Callback->second; item != NULL; item = item->Next)
	{
		item->Func(item->Data);
	}
}

void CSettings::RegisterChangeCB(SettingID Type,void * Data, SettingChangedFunc Func)
{
	SETTING_CHANGED_CB * new_item = new SETTING_CHANGED_CB;
	new_item->Data = Data;
	new_item->Func = Func;
	new_item->Next = NULL;


	SETTING_CALLBACK::iterator Callback = m_Callback.find(Type);
	if (Callback != m_Callback.end())
	{
		SETTING_CHANGED_CB * item = Callback->second;
		while (item->Next)
		{
			item = item->Next;
		}
		item->Next = new_item;			
	} else {
		m_Callback.insert(SETTING_CALLBACK::value_type(Type,new_item));
	}
}

void CSettings::UnregisterChangeCB(SettingID Type,void * Data, SettingChangedFunc Func)
{
	//Find out the information for handling the setting type from the list
/*	SETTING_MAP::iterator FindInfo = SettingInfo.find(Type);
	if (FindInfo == SettingInfo.end()) 
	{  //if not found do nothing
		UnknownSetting(Type);
		return; 
	}
	CSettingInfo * Info = &FindInfo->second;
	if (Info->DataType == Data_DWORD)
	{
		for (SETTING_CHANGED_CB_LIST::iterator iter = m_CBDwordList.begin(); iter != m_CBDwordList.end(); iter ++)
		{
			SETTING_CHANGED_CB & item = *iter;
			if (item.Type != Type) { continue; }
			if (item.Data != Data) { continue; }
			if (item.Func != Func) { continue; }
			m_CBDwordList.erase(iter);
			break;
		}
	}*/
}
