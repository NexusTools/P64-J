#include "../User Interface.h"
#include "../N64 System.h"
#include "../Plugin.h"
#include "..\3rd Party\Zip.h"
#include "..\3rd Party\7zip.h"
#include "../Support.h"
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <math.h>

CRomBrowser::CRomBrowser (WND_HANDLE & MainWindow, WND_HANDLE & StatusWindow, CNotification * Notify, CN64System * System) :
	m_MainWindow(MainWindow), 
	m_StatusWindow(StatusWindow),
	m_RefreshThread(NULL),
	m_RomIniFile(NULL),
	m_NotesIniFile(NULL),
	m_ExtIniFile(NULL),
	m_ZipIniFile(NULL),
	m_WatchThreadID(0),
	m_ShowingRomBrowser(false),
	_Notify(Notify),
	m_AllowSelectionLastRom(true),
	m_Plugins(NULL)
{
	if (_Settings) {
		m_RomIniFile = new CIniFile(_Settings->LoadString(RomDatabaseFile).c_str());
		m_NotesIniFile = new CIniFile(_Settings->LoadString(NotesIniName).c_str());
		m_ExtIniFile = new CIniFile(_Settings->LoadString(ExtIniName).c_str());
		m_ZipIniFile = new CIniFile(_Settings->LoadString(ZipCacheIniName).c_str());
	}
	_System = System;
	
	m_hRomList = 0;
	m_Visible  = false;
	m_WatchThread = NULL;
    m_WatchStopEvent = NULL;	


#define AddField(Name,Pos,ID,Width,LangID) m_Fields.push_back(ROMBROWSER_FIELDS(Name,Pos,ID,Width,LangID));
	
	AddField("File Name",              -1, RB_FileName,      218,RB_FILENAME);
	AddField("Internal Name",          -1, RB_InternalName,  200,RB_INTERNALNAME);
	AddField("Good Name",               0, RB_GoodName,      218,RB_GOODNAME);
	AddField("Status",                  1, RB_Status,        92, RB_STATUS);
	AddField("Rom Size",               -1, RB_RomSize,       100,RB_ROMSIZE);
	AddField("Notes (Core)",            2, RB_CoreNotes,     120,RB_NOTES_CORE);
	AddField("Notes (default plugins)", 3, RB_PluginNotes,   188,RB_NOTES_PLUGIN);
	AddField("Notes (User)",           -1, RB_UserNotes,     100,RB_NOTES_USER);
	AddField("Cartridge ID",           -1, RB_CartridgeID,   100,RB_CART_ID);
	AddField("Manufacturer",           -1, RB_Manufacturer,  100,RB_MANUFACTUER);
	AddField("Country",                -1, RB_Country,       100,RB_COUNTRY);
	AddField("Developer",              -1, RB_Developer,     100,RB_DEVELOPER);
	AddField("CRC1",                   -1, RB_Crc1,          100,RB_CRC1);
	AddField("CRC2",                   -1, RB_Crc2,          100,RB_CRC2);
	AddField("CIC Chip",               -1, RB_CICChip,       100,RB_CICCHIP);
	AddField("Release Date",           -1, RB_ReleaseDate,   100,RB_RELEASE_DATE);
	AddField("Genre",                  -1, RB_Genre,         100,RB_GENRE);
	AddField("Players",                -1, RB_Players,       100,RB_PLAYERS);
	AddField("Force Feedback",         -1, RB_ForceFeedback, 100,RB_FORCE_FEEDBACK);
	AddField("File Format",            -1, RB_FileFormat,    100,RB_FILE_FORMAT);

#undef AddField
	m_FieldType.resize(m_Fields.size());
	
	if (_Settings == NULL) { return; }
	
	//Load the real positions from the settings
	for (size_t Field = 0; Field < m_Fields.size(); Field++) 
	{
		_Settings->LoadDwordIndex(RomBrowserPosIndex,Field,(DWORD &)m_Fields[Field].Pos );
		_Settings->LoadDwordIndex(RomBrowserWidthIndex,Field,(DWORD &)m_Fields[Field].ColWidth);
	}
}

CRomBrowser::~CRomBrowser (void){
	m_StopRefresh = true;
	WatchThreadStop();
	DeallocateBrushs();

	if (m_RomIniFile)
	{
		delete m_RomIniFile;
		m_RomIniFile = NULL;
	}
	if (m_NotesIniFile)
	{
		delete m_NotesIniFile;
		m_NotesIniFile = NULL;
	}
	if (m_ExtIniFile)
	{
		delete m_ExtIniFile;
		m_ExtIniFile = NULL;
	}
	if (m_ZipIniFile)
	{
		delete m_ZipIniFile;
		m_ZipIniFile = NULL;
	}
}

int CRomBrowser::CalcSortPosition (DWORD lParam) 
{
	int Start = 0;
	int End   = ListView_GetItemCount((HWND)m_hRomList) - 1;
	if (End < 0)
	{
		return 0;
	}


	int count;
	for (count = NoOfSortKeys; count >= 0; count --) {
		stdstr SortFieldName = _Settings->LoadStringIndex(RomBrowserSortFieldIndex,count);
		if (SortFieldName.length() == 0)
		{
			continue;
		}
		
		if (End == Start)
		{
			break;
		}

		size_t index;
		for (index = 0; index < m_Fields.size(); index++) {
			if (_stricmp(m_Fields[index].Name,SortFieldName.c_str()) == 0) { break; }
		}
		if (index >= m_Fields.size()) { continue; }
		SORT_FIELD SortFieldInfo;
		SortFieldInfo._this     = this;
		SortFieldInfo.Key       = index;
		SortFieldInfo.KeyAscend = _Settings->LoadBoolIndex(RomBrowserSortAscendingIndex,count);

		//calc new start and end
		int LastTestPos = -1;
		while (Start < End)
		{
			int TestPos = (int)floor((float)((Start + End) / 2));
			if (LastTestPos == TestPos)
			{
				TestPos += 1;
			}
			LastTestPos = TestPos;

			LV_ITEM  lvItem;
			memset(&lvItem, 0, sizeof(LV_ITEM));
			lvItem.mask = LVIF_PARAM;
			lvItem.iItem = TestPos;
			if (!ListView_GetItem((HWND)m_hRomList, &lvItem)) { return End; }

			int Result = RomList_CompareItems(lParam,lvItem.lParam,(DWORD)&SortFieldInfo);
			if (Result < 0)
			{
				if (End == TestPos)
				{
					break;
				}
				End = TestPos;
			}
			else if (Result > 0)
			{
				if (Start == TestPos)
				{
					break;
				}
				Start = TestPos;
			}
			else
			{
				//Find new start
				float Left = (float)Start;
				float Right = (float)TestPos;
				while (Left < Right)
				{
					int NewTestPos = (int)floor((Left + Right) / 2);
					if (LastTestPos == NewTestPos)
					{
						NewTestPos += 1;
					}
					LastTestPos = NewTestPos;
						
					LV_ITEM  lvItem;
					memset(&lvItem, 0, sizeof(LV_ITEM));
					lvItem.mask = LVIF_PARAM;
					lvItem.iItem = NewTestPos;
					if (!ListView_GetItem((HWND)m_hRomList, &lvItem)) { return End; }

					int Result = RomList_CompareItems(lParam,lvItem.lParam,(DWORD)&SortFieldInfo);
					if (Result == 0)
					{
						if (Right == NewTestPos)
						{
							break;
						}
						Right = (float)NewTestPos;
					}
					if (Result > 0)
					{
						Left = NewTestPos;
					}

				}
				Start = Right;

				//Find new end
				Left = TestPos;
				Right = End;
				while (Left < Right)
				{
					int NewTestPos = ceil((Left + Right) / 2);
					if (LastTestPos == NewTestPos)
					{
						NewTestPos -= 1;
					}
					LastTestPos = NewTestPos;
					
					LV_ITEM  lvItem;
					memset(&lvItem, 0, sizeof(LV_ITEM));
					lvItem.mask = LVIF_PARAM;
					lvItem.iItem = NewTestPos;
					if (!ListView_GetItem((HWND)m_hRomList, &lvItem)) { return End; }

					int Result = RomList_CompareItems(lParam,lvItem.lParam,(DWORD)&SortFieldInfo);
					if (Result >= 0)
					{
						if (Left == NewTestPos)
						{
							break;
						}
						Left = NewTestPos;
					}
					if (Result < 0)
					{
						Right = NewTestPos;
					}

				}
				End = Left;
				break;
			}
			
		}

		
	}

	//Compare end with item to see if we should do it after or before it
	for (count = 0; count < NoOfSortKeys; count ++) {
		stdstr SortFieldName = _Settings->LoadStringIndex(RomBrowserSortFieldIndex,count);
		if (SortFieldName.length() == 0)
		{
			continue;
		}
		
		int index;
		for (index = 0; index < m_Fields.size(); index++) {
			if (_stricmp(m_Fields[index].Name,SortFieldName.c_str()) == 0) { break; }
		}
		if (index >= m_Fields.size()) { continue; }
		SORT_FIELD SortFieldInfo;
		SortFieldInfo._this     = this;
		SortFieldInfo.Key       = index;
		SortFieldInfo.KeyAscend = _Settings->LoadBoolIndex(RomBrowserSortAscendingIndex,count) != 0;

		LV_ITEM  lvItem;
		memset(&lvItem, 0, sizeof(LV_ITEM));
		lvItem.mask = LVIF_PARAM;
		lvItem.iItem = End;
		if (!ListView_GetItem((HWND)m_hRomList, &lvItem)) { return End; }

		int Result = RomList_CompareItems(lParam, lvItem.lParam ,(DWORD)&SortFieldInfo);
		if (Result < 0)
		{
			return End;
		}
		if (Result > 0)
		{
			return End + 1;
		}
	}
	return End + 1;
}

void CRomBrowser::AddRomToList (const char * RomLocation, const char * lpLastRom) {
	ROM_INFO RomInfo;	
	
	memset(&RomInfo, 0, sizeof(ROM_INFO));	
	strncpy(RomInfo.szFullFileName, RomLocation, sizeof(RomInfo.szFullFileName) - 1);
	if (!FillRomInfo(&RomInfo)) { return;  }
	AddRomInfoToList(RomInfo,lpLastRom);
}

void CRomBrowser::AddRomInfoToList (ROM_INFO &RomInfo , const char * lpLastRom) {	
	int ListPos = m_RomInfo.size();
	m_RomInfo.push_back(RomInfo);

	LV_ITEM  lvItem;
	memset(&lvItem, 0, sizeof(lvItem));
	lvItem.mask      = LVIF_TEXT | LVIF_PARAM;		
	lvItem.iItem     = CalcSortPosition(ListPos);
	lvItem.lParam    = (LPARAM)ListPos;
	lvItem.pszText   = LPSTR_TEXTCALLBACK;
		
	int index = ListView_InsertItem((HWND)m_hRomList, &lvItem);	

	
	int iItem = ListView_GetNextItem((HWND)m_hRomList, -1, LVNI_SELECTED);
	//if (iItem == -1) { return; }

	//if the last rom then highlight the item
	if (iItem < 0 && _stricmp(RomInfo.szFullFileName,lpLastRom) == 0)
	{
		ListView_SetItemState((HWND)m_hRomList,index,LVIS_SELECTED | LVIS_FOCUSED,LVIS_SELECTED | LVIS_FOCUSED);
	}

	if (iItem >= 0)
	{
		ListView_EnsureVisible((HWND)m_hRomList,iItem,FALSE);
	}
}

void CRomBrowser::AllocateBrushs (void) {
	for (int count = 0; count < m_RomInfo.size(); count++) {
		if (m_RomInfo[count].SelColor == -1) { 
			m_RomInfo[count].SelColorBrush = (DWORD)((HBRUSH)(COLOR_HIGHLIGHT + 1));
		} else {
			m_RomInfo[count].SelColorBrush = (DWORD)CreateSolidBrush(m_RomInfo[count].SelColor);
		}
	}
}

DWORD CRomBrowser::AsciiToHex (char * HexValue) {
	DWORD Count, Finish, Value = 0;

	Finish = strlen(HexValue);
	if (Finish > 8 ) { Finish = 8; }

	for (Count = 0; Count < Finish; Count++){
		Value = (Value << 4);
		switch( HexValue[Count] ) {
		case '0': break;
		case '1': Value += 1; break;
		case '2': Value += 2; break;
		case '3': Value += 3; break;
		case '4': Value += 4; break;
		case '5': Value += 5; break;
		case '6': Value += 6; break;
		case '7': Value += 7; break;
		case '8': Value += 8; break;
		case '9': Value += 9; break;
		case 'A': Value += 10; break;
		case 'a': Value += 10; break;
		case 'B': Value += 11; break;
		case 'b': Value += 11; break;
		case 'C': Value += 12; break;
		case 'c': Value += 12; break;
		case 'D': Value += 13; break;
		case 'd': Value += 13; break;
		case 'E': Value += 14; break;
		case 'e': Value += 14; break;
		case 'F': Value += 15; break;
		case 'f': Value += 15; break;
		default: 
			Value = (Value >> 4);
			Count = Finish;
		}
	}
	return Value;
}


void CRomBrowser::CreateRomListControl (void) {
	m_hRomList = (WND_HANDLE)CreateWindowEx( WS_EX_CLIENTEDGE,WC_LISTVIEW,NULL,
					WS_TABSTOP | WS_VISIBLE | WS_CHILD | LVS_OWNERDRAWFIXED |
					WS_BORDER | LVS_SINGLESEL | LVS_REPORT,
					0,0,0,0,(HWND)m_MainWindow,(HMENU)IDC_ROMLIST,GetModuleHandle(NULL),NULL);	
	ResetRomBrowserColomuns();
	LoadRomList();
}

void CRomBrowser::DeallocateBrushs (void) {
	for (int count = 0; count < m_RomInfo.size(); count++) {
		if (m_RomInfo[count].SelColor == -1) { 
			continue;
		} 
		if (m_RomInfo[count].SelColorBrush) {
			DeleteObject((HBRUSH)m_RomInfo[count].SelColorBrush);
			m_RomInfo[count].SelColorBrush = NULL;
		}
	}
}

void CRomBrowser::FillRomExtensionInfo(ROM_INFO * pRomInfo) {
	//Initialize the structure
	pRomInfo->UserNotes[0]   = 0;
	pRomInfo->Developer[0]   = 0;
	pRomInfo->ReleaseDate[0] = 0;
	pRomInfo->Genre[0]       = 0;
	pRomInfo->Players        = 1;
	pRomInfo->CoreNotes[0]   = 0;
	pRomInfo->PluginNotes[0] = 0;
	strcpy(pRomInfo->GoodName,GS(RB_NOT_GOOD_FILE));
	strcpy(pRomInfo->Status,"Unknown");

	//Get File Identifier
	char Identifier[100];
	sprintf(Identifier,"%08X-%08X-C:%X",pRomInfo->CRC1,pRomInfo->CRC2,pRomInfo->Country);

	//Rom Notes
	if (m_Fields[RB_UserNotes].Pos >= 0) {
		m_NotesIniFile->GetString(Identifier,"Note","",pRomInfo->UserNotes,sizeof(pRomInfo->UserNotes));
	}

	//Rom Extension info
	if (m_Fields[RB_Developer].Pos >= 0) {
		m_ExtIniFile->GetString(Identifier,"Developer","",pRomInfo->Developer,sizeof(pRomInfo->Developer));
	}
	if (m_Fields[RB_ReleaseDate].Pos >= 0) {
		m_ExtIniFile->GetString(Identifier,"ReleaseDate","",pRomInfo->ReleaseDate,sizeof(pRomInfo->ReleaseDate));
	}
	if (m_Fields[RB_Genre].Pos >= 0) {
		m_ExtIniFile->GetString(Identifier,"Genre","",pRomInfo->Genre,sizeof(pRomInfo->Genre));
	}
	if (m_Fields[RB_Players].Pos >= 0) {		
		 m_ExtIniFile->GetNumber(Identifier,"Players",1,(DWORD &)pRomInfo->Players);
	}
	if (m_Fields[RB_ForceFeedback].Pos >= 0) {
		m_ExtIniFile->GetString(Identifier,"ForceFeedback","unknown",pRomInfo->ForceFeedback,sizeof(pRomInfo->ForceFeedback));
	}

	//Rom Settings
	if (m_Fields[RB_GoodName].Pos >= 0) {
		m_RomIniFile->GetString(Identifier,"Good Name",pRomInfo->GoodName,pRomInfo->GoodName,sizeof(pRomInfo->GoodName));
	}
	m_RomIniFile->GetString(Identifier,"Status",pRomInfo->Status,pRomInfo->Status,sizeof(pRomInfo->Status));
	if (m_Fields[RB_CoreNotes].Pos >= 0) {
		m_RomIniFile->GetString(Identifier,"Core Note","",pRomInfo->CoreNotes,sizeof(pRomInfo->CoreNotes));
	}
	if (m_Fields[RB_PluginNotes].Pos >= 0) {
		m_RomIniFile->GetString(Identifier,"Plugin Note","",pRomInfo->PluginNotes,sizeof(pRomInfo->PluginNotes));
	}

	//Get the text color
	char String[100];
	m_RomIniFile->GetString("Rom Status",pRomInfo->Status,"000000",String,7);	
	pRomInfo->TextColor = (AsciiToHex(String) & 0xFFFFFF);
	pRomInfo->TextColor = (pRomInfo->TextColor & 0x00FF00) | ((pRomInfo->TextColor >> 0x10) & 0xFF) | ((pRomInfo->TextColor & 0xFF) << 0x10);
	
	//Get the selected color
	sprintf(String,"%s.Sel",pRomInfo->Status);
	m_RomIniFile->GetString("Rom Status",String,"FFFFFFFF",String,9);	
	int selcol = AsciiToHex(String);
	if (selcol < 0) { 
		pRomInfo->SelColor = - 1;
	} else {
		selcol = (AsciiToHex(String) & 0xFFFFFF);
		selcol = (selcol & 0x00FF00) | ((selcol >> 0x10) & 0xFF) | ((selcol & 0xFF) << 0x10);
		pRomInfo->SelColor = selcol;
	}

	//Get the selected text color
	sprintf(String,"%s.Seltext",pRomInfo->Status);
	m_RomIniFile->GetString("Rom Status",String,"FFFFFF",String,7);	
	pRomInfo->SelTextColor = (AsciiToHex(String) & 0xFFFFFF);
	pRomInfo->SelTextColor = (pRomInfo->SelTextColor & 0x00FF00) | ((pRomInfo->SelTextColor >> 0x10) & 0xFF) | ((pRomInfo->SelTextColor & 0xFF) << 0x10);
}

bool CRomBrowser::FillRomInfo(ROM_INFO * pRomInfo) {
	BYTE RomData[0x1000];

	if (!LoadDataFromRomFile(pRomInfo->szFullFileName,RomData,sizeof(RomData),&pRomInfo->RomSize,pRomInfo->FileFormat)) { return FALSE; }
	return FillRomInfo2(pRomInfo,RomData, sizeof(RomData));
}

bool CRomBrowser::FillRomInfo2(ROM_INFO * pRomInfo, BYTE * RomData, DWORD RomDataSize )
{
	int count;


	if (strstr(pRomInfo->szFullFileName,"?") != NULL)
	{
		strcpy(pRomInfo->FileName,strstr(pRomInfo->szFullFileName,"?") + 1);
	} else {
		char drive[_MAX_DRIVE] ,dir[_MAX_DIR], ext[_MAX_EXT];
		_splitpath( pRomInfo->szFullFileName, drive, dir, pRomInfo->FileName, ext );
	}
	if (m_Fields[RB_InternalName].Pos >= 0) {
		memcpy(pRomInfo->InternalName,(void *)(RomData + 0x20),20);
		for( count = 0 ; count < 20; count += 4 ) {
			pRomInfo->InternalName[count] ^= pRomInfo->InternalName[count+3];
			pRomInfo->InternalName[count + 3] ^= pRomInfo->InternalName[count];
			pRomInfo->InternalName[count] ^= pRomInfo->InternalName[count+3];			
			pRomInfo->InternalName[count + 1] ^= pRomInfo->InternalName[count + 2];
			pRomInfo->InternalName[count + 2] ^= pRomInfo->InternalName[count + 1];
			pRomInfo->InternalName[count + 1] ^= pRomInfo->InternalName[count + 2];			
		}
		pRomInfo->InternalName[21] = '\0';
	}
	pRomInfo->CartID[0] = *(RomData + 0x3F);
	pRomInfo->CartID[1] = *(RomData + 0x3E);
	pRomInfo->CartID[2] = '\0';
	pRomInfo->Manufacturer = *(RomData + 0x38);
	pRomInfo->Country = *(RomData + 0x3D);
	pRomInfo->CRC1 = *(DWORD *)(RomData + 0x10);
	pRomInfo->CRC2 = *(DWORD *)(RomData + 0x14);
	pRomInfo->CicChip = GetCicChipID(RomData);
	
	FillRomExtensionInfo(pRomInfo);
	
	if (pRomInfo->SelColor == -1) { 
		pRomInfo->SelColorBrush = (DWORD)((HBRUSH)(COLOR_HIGHLIGHT + 1));
	} else {
		pRomInfo->SelColorBrush = (DWORD)CreateSolidBrush(pRomInfo->SelColor);
	}
	
	return TRUE;
}

void CRomBrowser::GetRomFileNames( strlist & FileList, CPath & BaseDirectory, stdstr & Directory, bool InWatchThread )
{

	CPath SearchPath((const stdstr&)BaseDirectory,"*.*");
	SearchPath.AppendDirectory(Directory.c_str());

	if (!SearchPath.FindFirst(CPath::_A_ALLFILES))
	{
		return;
	}

	do {
		if (InWatchThread && WaitForSingleObject(m_WatchStopEvent,0) != WAIT_TIMEOUT)
		{
			return;
		}

		if (SearchPath.IsDirectory())
		{
			if (_Settings->LoadDword(RomBrowserRecursive)) 
			{
				stdstr CurrentDir = Directory + SearchPath.GetCurrentDirectory() + "\\";
				GetRomFileNames(FileList,BaseDirectory,CurrentDir,InWatchThread); 
			}
		} else {
			AddFileNameToList(FileList, Directory, SearchPath);
		}
	} while (SearchPath.FindNext());
}

void CRomBrowser::NotificationCB ( LPCSTR Status, CRomBrowser * _this )
{
	_this->_Notify->DisplayMessage(5,"%s",Status);
}


void CRomBrowser::AddFileNameToList( strlist & FileList, stdstr & Directory, CPath & File )
{
	if (FileList.size() > 3000)
	{
		return;
	}
	
	stdstr Drive, Dir, Name, Extension; 
	File.GetComponents(NULL,&Dir,&Name,&Extension);
	Extension.ToLower();
	if (Extension == "zip" || Extension == "7z"  || Extension == "v64" || Extension == "z64" || 
		Extension == "n64" || Extension == "rom" || Extension == "jap" || Extension == "pal" || 
		Extension == "usa" || Extension == "eur" || Extension == "bin")
	{
		stdstr FileName = Directory+Name+Extension;
		FileName.ToLower();
		FileList.push_back(FileName);
	}
}

void CRomBrowser::FillRomList ( strlist & FileList, CPath & BaseDirectory, stdstr & Directory, const char * lpLastRom )
{
	CPath SearchPath((const stdstr&)BaseDirectory,"*.*");
	SearchPath.AppendDirectory(Directory.c_str());

	WriteTraceF(TraceDebug,"CRomBrowser::FillRomList 1 %s",(LPCSTR)SearchPath);
	if (!SearchPath.FindFirst(CPath::_A_ALLFILES))
	{
		return;
	}

	do {
		WriteTraceF(TraceDebug,"CRomBrowser::FillRomList 2 m_StopRefresh = %d",m_StopRefresh);
		if (m_StopRefresh) { break; }

		if (SearchPath.IsDirectory())
		{
			if (_Settings->LoadDword(RomBrowserRecursive)) 
			{
				stdstr CurrentDir = Directory + SearchPath.GetCurrentDirectory() + "\\";
				FillRomList(FileList,BaseDirectory,CurrentDir,lpLastRom); 
			}
			continue;
		}

		AddFileNameToList(FileList, Directory, SearchPath);

		stdstr Extension = SearchPath.GetExtension();
		Extension.ToLower();

		if (Extension == "zip" || Extension == "v64" || Extension == "z64" || Extension == "n64" ||
			Extension == "rom" || Extension == "jap" || Extension == "pal" || Extension == "usa" ||
			Extension == "eur" || Extension == "bin") 
		{
			AddRomToList(SearchPath,lpLastRom); 
			continue; 
		}
		if (Extension == "7z" ) 
		{ 
			C7zip ZipFile(SearchPath);
			char ZipFileName[260];
			stdstr_f SectionName("%s-%d",ZipFile.FileName(ZipFileName,sizeof(ZipFileName)),ZipFile.FileSize());
			SectionName.ToLower();

			WriteTraceF(TraceDebug,"CRomBrowser::FillRomList 4 %s",SectionName.c_str());
			for (int i = 0; i < ZipFile.NumFiles(); i++)
			{
				CFileItem * f = ZipFile.FileItem(i);
		        if (f->IsDirectory)
				{
					continue;
				}
				ROM_INFO RomInfo;	
				
				WriteTrace(TraceDebug,"CRomBrowser::FillRomList 5");
				char drive2[_MAX_DRIVE] ,dir2[_MAX_DIR], FileName2[MAX_PATH], ext2[_MAX_EXT];
				_splitpath( f->Name, drive2, dir2, FileName2, ext2 );

				WriteTraceF(TraceDebug,"CRomBrowser::FillRomList 6 %s",ext2);
				if (_stricmp(ext2, ".v64") != 0 && _stricmp(ext2, ".z64") != 0 &&
					_stricmp(ext2, ".n64") != 0 && _stricmp(ext2, ".rom") != 0 &&
					_stricmp(ext2, ".jap") != 0 && _stricmp(ext2, ".pal") != 0 && 
					_stricmp(ext2, ".usa") != 0 && _stricmp(ext2, ".eur") != 0 &&
					_stricmp(ext2, ".bin") == 0)
				{
					continue;
				}
				WriteTrace(TraceDebug,"CRomBrowser::FillRomList 7");
				memset(&RomInfo, 0, sizeof(ROM_INFO));
				stdstr_f zipFileName("%s?%s",(LPCSTR)SearchPath,f->Name);
				ZipFile.SetNotificationCallback((C7zip::LP7ZNOTIFICATION)NotificationCB,this);

				strncpy(RomInfo.szFullFileName, zipFileName.c_str(), sizeof(RomInfo.szFullFileName) - 1);
				RomInfo.szFullFileName[sizeof(RomInfo.szFullFileName) - 1] = 0;
				strcpy(RomInfo.FileName,strstr(RomInfo.szFullFileName,"?") + 1);
				RomInfo.FileFormat = Format_7zip;

				WriteTrace(TraceDebug,"CRomBrowser::FillRomList 8");
				char szHeader[0x90];
				if (m_ZipIniFile->GetString(SectionName.c_str(),f->Name,"",szHeader,sizeof(szHeader)) == 0)
				{
					BYTE RomData[0x1000];
					ZipFile.GetFile(i,RomData,sizeof(RomData));
					
					WriteTrace(TraceDebug,"CRomBrowser::FillRomList 9");
					if (!IsValidRomImage(RomData)) { continue; }
					WriteTrace(TraceDebug,"CRomBrowser::FillRomList 10");
					ByteSwapRomData(RomData,sizeof(RomData));
					WriteTrace(TraceDebug,"CRomBrowser::FillRomList 11");
					
					stdstr RomHeader;
					for (int x = 0; x < 0x40; x += 4)
					{
						RomHeader += stdstr_f("%08X",*((DWORD *)&RomData[x]));
					}
					WriteTraceF(TraceDebug,"CRomBrowser::FillRomList 11a %s",RomHeader.c_str());
					int CicChip = GetCicChipID(RomData);

					//save this info
					WriteTrace(TraceDebug,"CRomBrowser::FillRomList 12");
					m_ZipIniFile->SaveString(SectionName.c_str(),f->Name,RomHeader.c_str());
					m_ZipIniFile->SaveNumber(SectionName.c_str(),stdstr_f("%s-Cic",f->Name).c_str(),CicChip);
					strcpy(szHeader,RomHeader.c_str());
				}
				WriteTrace(TraceDebug,"CRomBrowser::FillRomList 13");
				BYTE RomData[0x40];

				for (int x = 0; x < 0x40; x += 4)
				{
					*((DWORD *)&RomData[x]) = AsciiToHex(&szHeader[x*2]);
				}

				WriteTrace(TraceDebug,"CRomBrowser::FillRomList 14");
				memcpy(RomInfo.InternalName,(void *)(RomData + 0x20),20);
				for( int count = 0 ; count < 20; count += 4 ) {
					RomInfo.InternalName[count] ^= RomInfo.InternalName[count+3];
					RomInfo.InternalName[count + 3] ^= RomInfo.InternalName[count];
					RomInfo.InternalName[count] ^= RomInfo.InternalName[count+3];			
					RomInfo.InternalName[count + 1] ^= RomInfo.InternalName[count + 2];
					RomInfo.InternalName[count + 2] ^= RomInfo.InternalName[count + 1];
					RomInfo.InternalName[count + 1] ^= RomInfo.InternalName[count + 2];			
				}
				WriteTrace(TraceDebug,"CRomBrowser::FillRomList 15");
				RomInfo.InternalName[21] = '\0';
				RomInfo.CartID[0] = *(RomData + 0x3F);
				RomInfo.CartID[1] = *(RomData + 0x3E);
				RomInfo.CartID[2] = '\0';
				RomInfo.Manufacturer = *(RomData + 0x38);
				RomInfo.Country = *(RomData + 0x3D);
				RomInfo.CRC1 = *(DWORD *)(RomData + 0x10);
				RomInfo.CRC2 = *(DWORD *)(RomData + 0x14);
				m_ZipIniFile->GetNumber(SectionName.c_str(),stdstr_f("%s-Cic",f->Name).c_str(), -1,(DWORD &)RomInfo.CicChip);
				WriteTrace(TraceDebug,"CRomBrowser::FillRomList 16");
				FillRomExtensionInfo(&RomInfo);

				if (RomInfo.SelColor == -1) { 
					RomInfo.SelColorBrush = (DWORD)((HBRUSH)(COLOR_HIGHLIGHT + 1));
				} else {
					RomInfo.SelColorBrush = (DWORD)CreateSolidBrush(RomInfo.SelColor);
				}
				WriteTrace(TraceDebug,"CRomBrowser::FillRomList 17");
				AddRomInfoToList(RomInfo,lpLastRom);
			}
			continue; 
		}

	} while (SearchPath.FindNext());
}

int CRomBrowser::GetCicChipID (BYTE * RomData) {
	_int64 CRC = 0;
	int count;

	for (count = 0x40; count < 0x1000; count += 4) {
		CRC += *(DWORD *)(RomData+count);
	}
	switch (CRC) {
	case 0x000000D0027FDF31: return 1;
	case 0x000000CFFB631223: return 1;
	case 0x000000D057C85244: return 2;
	case 0x000000D6497E414B: return 3;
	case 0x0000011A49F60E96: return 5;
	case 0x000000D6D5BE5580: return 6;
	default:
		return -1;
	}
}

void CRomBrowser::HighLightLastRom(void) {
	if (!m_AllowSelectionLastRom)
	{
		return;
	}
	//Make sure Rom browser is visible
	if (!RomBrowserVisible()) { return; }

	//Get the string to the last rom
	stdstr LastRom = _Settings->LoadStringIndex(RecentRomFileIndex,0);
	LPCSTR lpLastRom = LastRom.c_str();

	LV_ITEM lvItem;
	lvItem.mask = LVIF_PARAM;

	int ItemCount = ListView_GetItemCount((HWND)m_hRomList);
	for (int index = 0; index < ItemCount; index++) 
	{
		//Get The next item
		lvItem.iItem = index;
		if (!ListView_GetItem((HWND)m_hRomList, &lvItem)) { return; }

		//Get the rom info for that item
		if (lvItem.lParam < 0 || lvItem.lParam >= m_RomInfo.size())
		{
			return;
		}
		ROM_INFO * pRomInfo = &m_RomInfo[lvItem.lParam];
		
		if (!m_AllowSelectionLastRom)
		{
			return;
		}

		//if the last rom then highlight the item
		if (_stricmp(pRomInfo->szFullFileName,lpLastRom) == 0) {
			ListView_SetItemState((HWND)m_hRomList,index,LVIS_SELECTED | LVIS_FOCUSED,LVIS_SELECTED | LVIS_FOCUSED);
			ListView_EnsureVisible((HWND)m_hRomList,index,FALSE);
			return;
		}
	}	
}

bool CRomBrowser::IsValidRomImage ( BYTE Test[4] ) {
	if ( *((DWORD *)&Test[0]) == 0x40123780 ) { return TRUE; }
	if ( *((DWORD *)&Test[0]) == 0x12408037 ) { return TRUE; }
	if ( *((DWORD *)&Test[0]) == 0x80371240 ) { return TRUE; }
	return FALSE;
}

bool CRomBrowser::LoadDataFromRomFile(char * FileName,BYTE * Data,int DataLen, int * RomSize, FILE_FORMAT & FileFormat) {
	BYTE Test[4];

	if (strnicmp(&FileName[strlen(FileName)-4], ".ZIP",4) == 0 ){ 
		int len, port = 0, FoundRom;
	    unz_file_info info;
		char zname[132];
		unzFile file;
		file = unzOpen(FileName);
		if (file == NULL) { return FALSE; }

		port = unzGoToFirstFile(file);
		FoundRom = FALSE; 
		while(port == UNZ_OK && FoundRom == FALSE) {
			unzGetCurrentFileInfo(file, &info, zname, 128, NULL,0, NULL,0);
		    if (unzLocateFile(file, zname, 1) != UNZ_OK ) {
				unzClose(file);
				return FALSE;
			}
			if( unzOpenCurrentFile(file) != UNZ_OK ) {
				unzClose(file);
				return FALSE;
			}
			unzReadCurrentFile(file,Test,4);
			if (IsValidRomImage(Test)) {
				FoundRom = TRUE;
				//RomFileSize = info.uncompressed_size;
				memcpy(Data,Test,4);
				len = unzReadCurrentFile(file,&Data[4],DataLen - 4) + 4;

				if ((int)DataLen != len) {
					unzCloseCurrentFile(file);
					unzClose(file);
					return FALSE;
				}
				*RomSize = info.uncompressed_size;
				if(unzCloseCurrentFile(file) == UNZ_CRCERROR) {
					unzClose(file);
					return FALSE;
				}
				unzClose(file);
			}
			if (FoundRom == FALSE) {
				unzCloseCurrentFile(file);
				port = unzGoToNextFile(file);
			}

		}
		if (FoundRom == FALSE) {
			return FALSE;
		}
		FileFormat = Format_Zip;
	} else {
		DWORD dwRead;
		HANDLE hFile;
		
		hFile = CreateFile(FileName,GENERIC_READ,FILE_SHARE_READ,NULL,
			OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
			NULL);
		
		if (hFile == INVALID_HANDLE_VALUE) {  return FALSE; }

		SetFilePointer(hFile,0,0,FILE_BEGIN);
		ReadFile(hFile,Test,4,&dwRead,NULL);
		if (!IsValidRomImage(Test)) { CloseHandle( hFile ); return FALSE; }
		SetFilePointer(hFile,0,0,FILE_BEGIN);
		if (!ReadFile(hFile,Data,DataLen,&dwRead,NULL)) { CloseHandle( hFile ); return FALSE; }
		*RomSize = GetFileSize(hFile,NULL);
		CloseHandle( hFile ); 		
		FileFormat = Format_Uncompressed;
	}	
	ByteSwapRomData(Data,DataLen);
	return TRUE;
}

void CRomBrowser::ByteSwapRomData (BYTE * Data, int DataLen)
{
	int count;

	switch (*((DWORD *)&Data[0])) {
	case 0x12408037:
		for( count = 0 ; count < DataLen; count += 4 ) {
			Data[count] ^= Data[count+2];
			Data[count + 2] ^= Data[count];
			Data[count] ^= Data[count+2];			
			Data[count + 1] ^= Data[count + 3];
			Data[count + 3] ^= Data[count + 1];
			Data[count + 1] ^= Data[count + 3];			
		}
		break;
	case 0x40123780:
		for( count = 0 ; count < DataLen; count += 4 ) {
			Data[count] ^= Data[count+3];
			Data[count + 3] ^= Data[count];
			Data[count] ^= Data[count+3];			
			Data[count + 1] ^= Data[count + 2];
			Data[count + 2] ^= Data[count + 1];
			Data[count + 1] ^= Data[count + 2];			
		}
		break;
	case 0x80371240: break;
	}
}

void CRomBrowser::LoadRomList (void) {
	stdstr FileName = _Settings->LoadString(RomListCache);
	
	//Open the cache file
	HANDLE hFile = CreateFile(FileName.c_str(),GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		//if file does not exist then refresh the data
		RefreshRomBrowser();
		return;
	}
	
	DWORD dwRead;
	unsigned char md5[16];
	ReadFile(hFile,&md5,sizeof(md5),&dwRead,NULL);

	//Read the size of ROM_INFO
	int   RomInfoSize = 0;
	ReadFile(hFile,&RomInfoSize,sizeof(RomInfoSize),&dwRead,NULL);
	if (RomInfoSize != sizeof(ROM_INFO) || dwRead != sizeof(RomInfoSize)) {
		CloseHandle(hFile);
		RefreshRomBrowser();
		return;
	}

	//Read the Number of entries
	int Entries = 0;
	ReadFile(hFile,&Entries,sizeof(Entries),&dwRead,NULL);

	//Read Every Entry
	DeallocateBrushs();
	m_RomInfo.clear();
	for (int count = 0; count < Entries; count++) {
		ROM_INFO RomInfo;
		ReadFile(hFile,&RomInfo,RomInfoSize,&dwRead,NULL);
		RomInfo.SelColorBrush = NULL;
			
		LV_ITEM  lvItem;
		memset(&lvItem, 0, sizeof(lvItem));
		lvItem.mask      = LVIF_TEXT | LVIF_PARAM;		
		lvItem.iItem     = ListView_GetItemCount((HWND)m_hRomList);
		lvItem.lParam    = (LPARAM)m_RomInfo.size();
		lvItem.pszText   = LPSTR_TEXTCALLBACK;
			
		int index = ListView_InsertItem((HWND)m_hRomList, &lvItem);	
		m_RomInfo.push_back(RomInfo);
	}
	CloseHandle(hFile);
	AllocateBrushs();
	RomList_SortList();
}

void CRomBrowser::MenuSetText ( MENU_HANDLE hMenu, int MenuPos, const char * Title, char * ShotCut) {
	MENUITEMINFO MenuInfo;
	char String[256];

	if (Title == NULL || strlen(Title) == 0) { return; }

	memset(&MenuInfo, 0, sizeof(MENUITEMINFO));
	MenuInfo.cbSize = sizeof(MENUITEMINFO);
	MenuInfo.fMask = MIIM_TYPE;
	MenuInfo.fType = MFT_STRING;
	MenuInfo.fState = MFS_ENABLED;
	MenuInfo.dwTypeData = String;
	MenuInfo.cch = 256;

	GetMenuItemInfo((HMENU)hMenu,MenuPos,TRUE,&MenuInfo);
	strcpy(String,Title);
	if (strchr(String,'\t') != NULL) { *(strchr(String,'\t')) = '\0'; }
	if (ShotCut) { sprintf(String,"%s\t%s",String,ShotCut); }
	SetMenuItemInfo((HMENU)hMenu,MenuPos,TRUE,&MenuInfo);
}

void CRomBrowser::RefreshRomBrowser (void) {
	DWORD ThreadID;

	if (m_RefreshThread)
	{
		return;
	}
	WriteTrace(TraceDebug,"CRomBrowser::RefreshRomBrowser 1");
	m_StopRefresh = false;
	m_RefreshThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)RefreshRomBrowserStatic,(LPVOID)this,0,&ThreadID);
	WriteTrace(TraceDebug,"CRomBrowser::RefreshRomBrowser 2");
}

void CRomBrowser::RefreshRomBrowserStatic (CRomBrowser * _this) 
{
	try
	{		
		if (_this->m_hRomList == NULL) { return; }

		//delete cache
		stdstr CacheFileName = _Settings->LoadString(RomListCache);
		DeleteFile(CacheFileName.c_str());

		//clear all current items
		WriteTrace(TraceDebug,"CRomBrowser::RefreshRomBrowserStatic 1");
		ListView_DeleteAllItems((HWND)_this->m_hRomList);
		_this->DeallocateBrushs();
		_this->m_RomInfo.clear();
		WriteTrace(TraceDebug,"CRomBrowser::RefreshRomBrowserStatic 2");
		InvalidateRect((HWND)_this->m_hRomList,NULL,TRUE);
		Sleep(100);
		WriteTrace(TraceDebug,"CRomBrowser::RefreshRomBrowserStatic 3");

		if (_this->m_WatchRomDir != _Settings->LoadString(RomDirectory))
		{
			WriteTrace(TraceDebug,"CRomBrowser::RefreshRomBrowserStatic 4");
			_this->WatchThreadStop();
			WriteTrace(TraceDebug,"CRomBrowser::RefreshRomBrowserStatic 5");
			_this->WatchThreadStart();
			WriteTrace(TraceDebug,"CRomBrowser::RefreshRomBrowserStatic 6");
		}

		WriteTrace(TraceDebug,"CRomBrowser::RefreshRomBrowserStatic 7");
		stdstr RomDir  = _Settings->LoadString(RomDirectory);
		stdstr LastRom = _Settings->LoadStringIndex(RecentRomFileIndex,0);
		WriteTrace(TraceDebug,"CRomBrowser::RefreshRomBrowserStatic 8");
		
		strlist FileNames;
		_this->FillRomList (FileNames, CPath(RomDir),stdstr(""), LastRom.c_str());
		WriteTrace(TraceDebug,"CRomBrowser::RefreshRomBrowserStatic 9");
		_this->SaveRomList(FileNames);
		WriteTrace(TraceDebug,"CRomBrowser::RefreshRomBrowserStatic 10");
		CloseHandle(_this->m_RefreshThread);
		_this->m_RefreshThread = NULL;
		WriteTrace(TraceDebug,"CRomBrowser::RefreshRomBrowserStatic 11");
	}
	catch (...)
	{
		WriteTraceF(TraceError,_T("CRomBrowser::RefreshRomBrowserStatic(): Unhandled Exception "));
	}
}




void CRomBrowser::ResetRomBrowserColomuns (void) {
	int Coloumn, index;
	LV_COLUMN lvColumn;
	char szString[300];

    //Remove all current coloumns
	memset(&lvColumn,0,sizeof(lvColumn));
	lvColumn.mask = LVCF_FMT;
	while (ListView_GetColumn((HWND)m_hRomList,0,&lvColumn)) {
		ListView_DeleteColumn((HWND)m_hRomList,0);
	}

	//Add Colomuns
	lvColumn.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvColumn.fmt = LVCFMT_LEFT;
	lvColumn.pszText = szString;

	for (Coloumn = 0; Coloumn < m_Fields.size(); Coloumn ++) {
		for (index = 0; index < m_Fields.size(); index ++) {
			if (m_Fields[index].Pos == Coloumn) { break; }
		}
		if (index == m_Fields.size() || m_Fields[index].Pos != Coloumn) {
			m_FieldType[Coloumn] = -1;
			break;
		}
		m_FieldType[Coloumn] = m_Fields[index].ID;
		lvColumn.cx = m_Fields[index].ColWidth;
		strncpy(szString, GS(m_Fields[index].LangID), sizeof(szString));
		ListView_InsertColumn((HWND)m_hRomList, Coloumn, &lvColumn);
	}
}

void CRomBrowser::ResizeRomList (WORD nWidth, WORD nHeight) {
	if (RomBrowserVisible()) {
		if (_Settings->LoadDword(RomBrowserMaximized) == 0 && nHeight != 0) {
			_Settings->SaveDword(RomBrowserWidth,nWidth);
			_Settings->SaveDword(RomBrowserHeight,nHeight);
		}		
		if (IsWindow((HWND)m_StatusWindow)) {
			RECT rc;

			GetWindowRect((HWND)m_StatusWindow, &rc);
			nHeight -= (WORD)(rc.bottom - rc.top);
		}
		MoveWindow((HWND)m_hRomList, 0, 0, nWidth, nHeight, TRUE);
	}
}

bool CRomBrowser::RomBrowserVisible (void) {
	if (!IsWindow((HWND)m_hRomList)) { return false; }
	if (!IsWindowVisible((HWND)m_hRomList)) { return false; }
	if (!m_Visible) { return false; }
	return true;
}

void CRomBrowser::RomBrowserToTop(void) {
	BringWindowToTop((HWND)m_hRomList);
	SetFocus((HWND)m_hRomList);
}

void CRomBrowser::RomBrowserMaximize(bool Mazimize) {
	_Settings->SaveDword(RomBrowserMaximized,(DWORD)Mazimize);
}

bool CRomBrowser::RomListDrawItem(int idCtrl, DWORD lParam) {
	if (idCtrl != IDC_ROMLIST) { return false; }
	LPDRAWITEMSTRUCT ditem = (LPDRAWITEMSTRUCT)lParam;

	RECT rcItem, rcDraw;
	char String[300];
	LV_ITEM lvItem;
	BOOL bSelected;
	HBRUSH hBrush;
    LV_COLUMN lvc; 
	int nColumn;

	lvItem.mask = LVIF_PARAM;
	lvItem.iItem = ditem->itemID;
	if (!ListView_GetItem((HWND)m_hRomList, &lvItem)) { return false; }
	lvItem.state = ListView_GetItemState((HWND)m_hRomList, ditem->itemID, -1);
	bSelected = (lvItem.state & LVIS_SELECTED);

	if (lvItem.lParam < 0 || lvItem.lParam >= m_RomInfo.size())
	{
		return true;
	}
	ROM_INFO * pRomInfo = &m_RomInfo[lvItem.lParam];
	if (pRomInfo == NULL)
	{
		return true;
	}
	if (bSelected) {
		hBrush = (HBRUSH)pRomInfo->SelColorBrush;
		SetTextColor(ditem->hDC,pRomInfo->SelTextColor);
	} else {
		hBrush = (HBRUSH)(COLOR_WINDOW + 1);
		SetTextColor(ditem->hDC,pRomInfo->TextColor);
	}
	FillRect( ditem->hDC, &ditem->rcItem,hBrush);	
	SetBkMode( ditem->hDC, TRANSPARENT );
	
	//Draw
	ListView_GetItemRect((HWND)m_hRomList,ditem->itemID,&rcItem,LVIR_LABEL);
	ListView_GetItemText((HWND)m_hRomList,ditem->itemID, 0, String, sizeof(String)); 
	memcpy(&rcDraw,&rcItem,sizeof(RECT));
	rcDraw.right -= 3;
	DrawText(ditem->hDC, String, strlen(String), &rcDraw, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER);	
	
    memset(&lvc,0,sizeof(lvc));
	lvc.mask = LVCF_FMT | LVCF_WIDTH; 
	for(nColumn = 1; ListView_GetColumn((HWND)m_hRomList,nColumn,&lvc); nColumn += 1) {		
		rcItem.left = rcItem.right; 
        rcItem.right += lvc.cx; 

		ListView_GetItemText((HWND)m_hRomList,ditem->itemID, nColumn, String, sizeof(String)); 
		memcpy(&rcDraw,&rcItem,sizeof(RECT));
		rcDraw.right -= 3;
		DrawText(ditem->hDC, String, strlen(String), &rcDraw, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER);
	}
	return true;
}

bool CRomBrowser::RomListNotify(int idCtrl, DWORD pnmh) {
	if (idCtrl != IDC_ROMLIST) { return false; }
	if (!RomBrowserVisible()) { return false; }

//				ListView_SetItemState((HWND)m_hRomList,index,LVIS_SELECTED | LVIS_FOCUSED,LVIS_SELECTED | LVIS_FOCUSED);

	switch (((LPNMHDR)pnmh)->code) {
	case LVN_COLUMNCLICK: RomList_ColoumnSortList(pnmh); break;
	case NM_RETURN:       RomList_OpenRom(pnmh); break;
	case NM_DBLCLK:       RomList_OpenRom(pnmh); break;
	case LVN_GETDISPINFO: RomList_GetDispInfo(pnmh); break;
	case NM_RCLICK:       RomList_PopupMenu(pnmh); break;
	case NM_CLICK:
		{
			LONG iItem = ListView_GetNextItem((HWND)m_hRomList, -1, LVNI_SELECTED);
			if (iItem != -1) 
			{ 
				m_AllowSelectionLastRom = false; 
			}
		}
        break;
	default:
		return false;
	}
	return true;
}

void CRomBrowser::RomList_ColoumnSortList(DWORD pnmh) {
	LPNMLISTVIEW pnmv = (LPNMLISTVIEW)pnmh;
	int index;

	for (index = 0; index < m_Fields.size(); index++) {
		if (m_Fields[index].Pos == pnmv->iSubItem) { break; }
	}
	if (m_Fields.size() == index) { return; }	
	if (_stricmp(_Settings->LoadStringIndex(RomBrowserSortFieldIndex,0).c_str(),m_Fields[index].Name) == 0) {
		_Settings->SaveBoolIndex(RomBrowserSortAscendingIndex,0,!_Settings->LoadBoolIndex(RomBrowserSortAscendingIndex,0));
	} else {
		int count;

		for (count = NoOfSortKeys; count > 0; count --) {
			_Settings->SaveStringIndex(RomBrowserSortFieldIndex,count,_Settings->LoadStringIndex(RomBrowserSortFieldIndex,count - 1).c_str());
			_Settings->SaveBoolIndex(RomBrowserSortAscendingIndex,count,_Settings->LoadBoolIndex(RomBrowserSortAscendingIndex, count - 1));
		}
		_Settings->SaveStringIndex(RomBrowserSortFieldIndex,0,m_Fields[index].Name);
		_Settings->SaveBoolIndex(RomBrowserSortAscendingIndex,0,true);
	}
	RomList_SortList();
}

int CALLBACK CRomBrowser::RomList_CompareItems(DWORD lParam1, DWORD lParam2, DWORD lParamSort) {
	SORT_FIELD * SortFieldInfo = (SORT_FIELD *)lParamSort;
	CRomBrowser * _this = SortFieldInfo->_this;
	if (lParam1 < 0 || lParam1 >= _this->m_RomInfo.size())
	{
		return 0;
	}
	if (lParam2 < 0 || lParam2 >= _this->m_RomInfo.size())
	{
		return 0;
	}
	ROM_INFO * pRomInfo1 = &_this->m_RomInfo[SortFieldInfo->KeyAscend?lParam1:lParam2];
	ROM_INFO * pRomInfo2 = &_this->m_RomInfo[SortFieldInfo->KeyAscend?lParam2:lParam1];
	int result;

	switch (SortFieldInfo->Key) {
	case RB_FileName: result = (int)lstrcmpi(pRomInfo1->FileName, pRomInfo2->FileName); break;
	case RB_InternalName: result =  (int)lstrcmpi(pRomInfo1->InternalName, pRomInfo2->InternalName); break;
	case RB_GoodName: result =  (int)lstrcmpi(pRomInfo1->GoodName, pRomInfo2->GoodName); break;
	case RB_Status: result =  (int)lstrcmpi(pRomInfo1->Status, pRomInfo2->Status); break;
	case RB_RomSize: result =  (int)pRomInfo1->RomSize - (int)pRomInfo2->RomSize; break;
	case RB_CoreNotes: result =  (int)lstrcmpi(pRomInfo1->CoreNotes, pRomInfo2->CoreNotes); break;
	case RB_PluginNotes: result =  (int)lstrcmpi(pRomInfo1->PluginNotes, pRomInfo2->PluginNotes); break;
	case RB_UserNotes: result =  (int)lstrcmpi(pRomInfo1->UserNotes, pRomInfo2->UserNotes); break;
	case RB_CartridgeID: result =  (int)lstrcmpi(pRomInfo1->CartID, pRomInfo2->CartID); break;
	case RB_Manufacturer: result =  (int)pRomInfo1->Manufacturer - (int)pRomInfo2->Manufacturer; break;
	case RB_Country: result =  (int)pRomInfo1->Country - (int)pRomInfo2->Country; break;
	case RB_Developer: result =  (int)lstrcmpi(pRomInfo1->Developer, pRomInfo2->Developer); break;
	case RB_Crc1: result =  (int)pRomInfo1->CRC1 - (int)pRomInfo2->CRC1; break;
	case RB_Crc2: result =  (int)pRomInfo1->CRC2 - (int)pRomInfo2->CRC2; break;
	case RB_CICChip: result =  (int)pRomInfo1->CicChip - (int)pRomInfo2->CicChip; break;
	case RB_ReleaseDate: result =  (int)lstrcmpi(pRomInfo1->ReleaseDate, pRomInfo2->ReleaseDate); break;
	case RB_Players: result =  (int)pRomInfo1->Players - (int)pRomInfo2->Players; break;
	case RB_ForceFeedback: result = (int)lstrcmpi(pRomInfo1->ForceFeedback, pRomInfo2->ForceFeedback); break;
	case RB_Genre: result =  (int)lstrcmpi(pRomInfo1->Genre, pRomInfo2->Genre); break;
	case RB_FileFormat: result =  (int)pRomInfo1->FileFormat - (int)pRomInfo2->FileFormat; break;
	default: result = 0; break;
	}
	return result;
}

void CRomBrowser::RomList_GetDispInfo(DWORD pnmh) {
	LV_DISPINFO * lpdi = (LV_DISPINFO *)pnmh;
	if (lpdi->item.lParam < 0 || lpdi->item.lParam >= m_RomInfo.size())
	{
		return;
	}

	ROM_INFO * pRomInfo = &m_RomInfo[lpdi->item.lParam];

	if (pRomInfo == NULL) 
	{
		strcpy(lpdi->item.pszText," "); 
		return;
	}

	switch(m_FieldType[lpdi->item.iSubItem]) {
	case RB_FileName: strncpy(lpdi->item.pszText, pRomInfo->FileName, lpdi->item.cchTextMax); break;
	case RB_InternalName: strncpy(lpdi->item.pszText, pRomInfo->InternalName, lpdi->item.cchTextMax); break;
	case RB_GoodName: strncpy(lpdi->item.pszText, pRomInfo->GoodName, lpdi->item.cchTextMax); break;
	case RB_CoreNotes: strncpy(lpdi->item.pszText, pRomInfo->CoreNotes, lpdi->item.cchTextMax); break;
	case RB_PluginNotes: strncpy(lpdi->item.pszText, pRomInfo->PluginNotes, lpdi->item.cchTextMax); break;
	case RB_Status: strncpy(lpdi->item.pszText, pRomInfo->Status, lpdi->item.cchTextMax); break;
	case RB_RomSize: sprintf(lpdi->item.pszText,"%.1f MBit",(float)pRomInfo->RomSize/0x20000); break;
	case RB_CartridgeID: strncpy(lpdi->item.pszText, pRomInfo->CartID, lpdi->item.cchTextMax); break;
	case RB_Manufacturer:
		switch (pRomInfo->Manufacturer) {
		case 'N':strncpy(lpdi->item.pszText, "Nintendo", lpdi->item.cchTextMax); break;
		case 0:  strncpy(lpdi->item.pszText, "None", lpdi->item.cchTextMax); break;
		default: sprintf(lpdi->item.pszText, "(Unknown %c (%X))", pRomInfo->Manufacturer,pRomInfo->Manufacturer); break;
		}
		break;		
	case RB_Country:
		switch (pRomInfo->Country) {
		case '7': strncpy(lpdi->item.pszText, "Beta", lpdi->item.cchTextMax); break;
		case 'A': strncpy(lpdi->item.pszText, "NTSC", lpdi->item.cchTextMax); break;
		case 'D': strncpy(lpdi->item.pszText, "Germany", lpdi->item.cchTextMax); break;
		case 'E': strncpy(lpdi->item.pszText, "America", lpdi->item.cchTextMax); break;
		case 'F': strncpy(lpdi->item.pszText, "France", lpdi->item.cchTextMax); break;
		case 'J': strncpy(lpdi->item.pszText, "Japan", lpdi->item.cchTextMax); break;
		case 'I': strncpy(lpdi->item.pszText, "Italy", lpdi->item.cchTextMax); break;
		case 'P': strncpy(lpdi->item.pszText, "Europe", lpdi->item.cchTextMax); break;
		case 'S': strncpy(lpdi->item.pszText, "Spain", lpdi->item.cchTextMax); break;
		case 'U': strncpy(lpdi->item.pszText, "Australia", lpdi->item.cchTextMax); break;
		case 'X': strncpy(lpdi->item.pszText, "PAL", lpdi->item.cchTextMax); break;
		case 'Y': strncpy(lpdi->item.pszText, "PAL", lpdi->item.cchTextMax); break;
		case 0: strncpy(lpdi->item.pszText, "None", lpdi->item.cchTextMax); break;
		default: sprintf(lpdi->item.pszText, "Unknown %c (%02X)", pRomInfo->Country,pRomInfo->Country); break;
		}
		break;			
	case RB_Crc1: sprintf(lpdi->item.pszText,"0x%08X",pRomInfo->CRC1); break;
	case RB_Crc2: sprintf(lpdi->item.pszText,"0x%08X",pRomInfo->CRC2); break;
	case RB_CICChip: 
		if (pRomInfo->CicChip < 0) { 
			sprintf(lpdi->item.pszText,"Unknown CIC Chip"); 
		} else {
			sprintf(lpdi->item.pszText,"CIC-NUS-610%d",pRomInfo->CicChip); 
		}
		break;
	case RB_UserNotes: strncpy(lpdi->item.pszText, pRomInfo->UserNotes, lpdi->item.cchTextMax); break;
	case RB_Developer: strncpy(lpdi->item.pszText, pRomInfo->Developer, lpdi->item.cchTextMax); break;
	case RB_ReleaseDate: strncpy(lpdi->item.pszText, pRomInfo->ReleaseDate, lpdi->item.cchTextMax); break;
	case RB_Genre: strncpy(lpdi->item.pszText, pRomInfo->Genre, lpdi->item.cchTextMax); break;
	case RB_Players: sprintf(lpdi->item.pszText,"%d",pRomInfo->Players); break;
	case RB_ForceFeedback: strncpy(lpdi->item.pszText, pRomInfo->ForceFeedback, lpdi->item.cchTextMax); break;
	case RB_FileFormat:
		switch (pRomInfo->FileFormat) {
		case Format_Uncompressed: strncpy(lpdi->item.pszText, "Uncompressed", lpdi->item.cchTextMax); break;
		case Format_Zip:          strncpy(lpdi->item.pszText, "Zip", lpdi->item.cchTextMax); break;
		case Format_7zip:         strncpy(lpdi->item.pszText, "7zip", lpdi->item.cchTextMax); break;
		default: sprintf(lpdi->item.pszText, "Unknown (%X)", pRomInfo->FileFormat); break;
		}
		break;		
	default: strncpy(lpdi->item.pszText, " ", lpdi->item.cchTextMax);
	}
	if (strlen(lpdi->item.pszText) == 0) { strcpy(lpdi->item.pszText," "); }
}

void CRomBrowser::RomList_OpenRom(DWORD pnmh) {
	ROM_INFO * pRomInfo;
	LV_ITEM lvItem;
	LONG iItem;

	iItem = ListView_GetNextItem((HWND)m_hRomList, -1, LVNI_SELECTED);
	if (iItem == -1) { return; }

	memset(&lvItem, 0, sizeof(LV_ITEM));
	lvItem.mask = LVIF_PARAM;
	lvItem.iItem = iItem;
	if (!ListView_GetItem((HWND)m_hRomList, &lvItem)) { return; }
	if (lvItem.lParam < 0 || lvItem.lParam >= m_RomInfo.size())
	{
		return;
	}
	pRomInfo = &m_RomInfo[lvItem.lParam];

	if (!pRomInfo) { return; }
	m_StopRefresh = true;
	_System->RunFileImage(pRomInfo->szFullFileName);
}

void CRomBrowser::RomList_PopupMenu(DWORD pnmh) {
	LV_DISPINFO * lpdi = (LV_DISPINFO *)pnmh;

	LONG iItem = ListView_GetNextItem((HWND)m_hRomList, -1, LVNI_SELECTED);
	m_SelectedRom = "";
	if (iItem != -1) { 
		LV_ITEM lvItem;
		memset(&lvItem, 0, sizeof(LV_ITEM));
		lvItem.mask = LVIF_PARAM;
		lvItem.iItem = iItem;
		if (!ListView_GetItem((HWND)m_hRomList, &lvItem)) { return; }
		if (lvItem.lParam < 0 || lvItem.lParam >= m_RomInfo.size())
		{
			return;
		}
		ROM_INFO * pRomInfo = &m_RomInfo[lvItem.lParam];

		if (!pRomInfo) { return; }
		m_SelectedRom = pRomInfo->szFullFileName;
	} 
	
	//Load the menu
	HMENU hMenu = LoadMenu(GetModuleHandle(NULL),MAKEINTRESOURCE(IDR_POPUP));
	MENU_HANDLE hPopupMenu = (MENU_HANDLE)GetSubMenu(hMenu,0);
	
	//Fix up menu
	MenuSetText(hPopupMenu, 0, GS(POPUP_PLAY), NULL);
	MenuSetText(hPopupMenu, 2, GS(MENU_REFRESH), NULL);
	MenuSetText(hPopupMenu, 3, GS(MENU_CHOOSE_ROM), NULL);
	MenuSetText(hPopupMenu, 5, GS(POPUP_INFO), NULL);
	MenuSetText(hPopupMenu, 6, GS(POPUP_GFX_PLUGIN), NULL);
	MenuSetText(hPopupMenu, 8, GS(POPUP_SETTINGS), NULL);
	MenuSetText(hPopupMenu, 9, GS(POPUP_CHEATS), NULL);

	if (m_SelectedRom.size() == 0) {
		DeleteMenu((HMENU)hPopupMenu,9,MF_BYPOSITION);
		DeleteMenu((HMENU)hPopupMenu,8,MF_BYPOSITION);
		DeleteMenu((HMENU)hPopupMenu,7,MF_BYPOSITION);
		DeleteMenu((HMENU)hPopupMenu,6,MF_BYPOSITION);
		DeleteMenu((HMENU)hPopupMenu,5,MF_BYPOSITION);
		DeleteMenu((HMENU)hPopupMenu,4,MF_BYPOSITION);
		DeleteMenu((HMENU)hPopupMenu,1,MF_BYPOSITION);
		DeleteMenu((HMENU)hPopupMenu,0,MF_BYPOSITION);
	} else {
		bool inBasicMode      = _Settings->LoadDword(BasicMode) != 0;
		bool CheatsRemembered = _Settings->LoadDword(RememberCheats) != 0;
		if (!CheatsRemembered) { DeleteMenu((HMENU)hPopupMenu,9,MF_BYPOSITION); }
		if (inBasicMode) { DeleteMenu((HMENU)hPopupMenu,8,MF_BYPOSITION); }
		if (inBasicMode && !CheatsRemembered) { DeleteMenu((HMENU)hPopupMenu,7,MF_BYPOSITION); }
		DeleteMenu((HMENU)hPopupMenu,6,MF_BYPOSITION); 
		if (!inBasicMode && m_Plugins && m_Plugins->Gfx() && m_Plugins->Gfx()->GetRomBrowserMenu != NULL)
		{
			HMENU GfxMenu = (HMENU)m_Plugins->Gfx()->GetRomBrowserMenu();
			if (GfxMenu)
			{
				MENUITEMINFO lpmii;
				InsertMenu ((HMENU)hPopupMenu, 6, MF_POPUP|MF_BYPOSITION, (DWORD)GfxMenu, GS(POPUP_GFX_PLUGIN));
				lpmii.cbSize = sizeof(MENUITEMINFO);
				lpmii.fMask = MIIM_STATE;			
				lpmii.fState = 0;
				SetMenuItemInfo((HMENU)hPopupMenu, (DWORD)GfxMenu, MF_BYCOMMAND,&lpmii);
			}
		}
	}
	
	//Get the current Mouse location
	POINT Mouse;
	GetCursorPos(&Mouse);

	//Show the menu
	TrackPopupMenu((HMENU)hPopupMenu, 0, Mouse.x, Mouse.y, 0,(HWND)m_MainWindow, NULL);
	DestroyMenu(hMenu);
}

void CRomBrowser::RomList_SortList(void) {
	SORT_FIELD SortFieldInfo;

	for (int count = NoOfSortKeys; count >= 0; count --) {
		stdstr SortFieldName = _Settings->LoadStringIndex(RomBrowserSortFieldIndex,count);
		
		int index;
		for (index = 0; index < m_Fields.size(); index++) {
			if (_stricmp(m_Fields[index].Name,SortFieldName.c_str()) == 0) { break; }
		}
		if (index >= m_Fields.size()) { continue; }
		SortFieldInfo._this     = this;
		SortFieldInfo.Key       = index;
		SortFieldInfo.KeyAscend = _Settings->LoadBoolIndex(RomBrowserSortAscendingIndex,count) != 0;
		ListView_SortItems((HWND)m_hRomList, RomList_CompareItems, &SortFieldInfo );
	}
}


/*
 * 	SaveRomList - save all the rom information about the current roms in the rom brower
 *                to a cache file, so it is quick to reload the information
 */
void CRomBrowser::SaveRomList ( strlist & FileList )
{
	MD5 ListHash = RomListHash(FileList);
	
/*	stdstr FileName = _Settings->LoadString(RomListCache);
	
	FileList.ToLower();
	WriteTraceF(TraceDebug,"SaveRomList: %s",FileList.c_str());
	MD5 md5((const unsigned char *)FileList.c_str(), FileList.length());
*/
	stdstr FileName = _Settings->LoadString(RomListCache);
	HANDLE hFile = CreateFile(FileName.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);

	DWORD dwWritten;
	WriteFile(hFile,ListHash.raw_digest(),16,&dwWritten,NULL);
	
	//Write the size of ROM_INFO
	int RomInfoSize = sizeof(ROM_INFO);
	WriteFile(hFile,&RomInfoSize,sizeof(RomInfoSize),&dwWritten,NULL);
	
	//Write the Number of entries
	int Entries = m_RomInfo.size();
	WriteFile(hFile,&Entries,sizeof(Entries),&dwWritten,NULL);

	//Write Every Entry
	for (int count = 0; count < Entries; count++) {
		ROM_INFO * RomInfo = &m_RomInfo[count];
		WriteFile(hFile,RomInfo,RomInfoSize,&dwWritten,NULL);
	}
	
	//Close the file handle
	CloseHandle(hFile);
}

void CRomBrowser::SaveRomListColoumnInfo(void) {
	WriteTrace(TraceDebug,"SaveRomListColoumnInfo - Start");
	//	if (!RomBrowserVisible()) { return; }
	if (_Settings == NULL) { return; }

	LV_COLUMN lvColumn;	

	memset(&lvColumn,0,sizeof(lvColumn));
	lvColumn.mask = LVCF_WIDTH;
	
	for (int Coloumn = 0;ListView_GetColumn((HWND)m_hRomList,Coloumn,&lvColumn); Coloumn++) {
		int index = 0;
		for (; index < m_Fields.size(); index++) {
			if (m_Fields[index].Pos == Coloumn) { break; }
		}
		m_Fields[index].ColWidth = lvColumn.cx;
	}

	//Save the real positions from the settings
	WriteTrace(TraceDebug,"SaveRomListColoumnInfo - Update Settings");
	for (int Field = 0; Field < m_Fields.size(); Field++) {
		_Settings->SaveDwordIndex(RomBrowserPosIndex,Field,m_Fields[Field].Pos );
		_Settings->SaveDwordIndex(RomBrowserWidthIndex,Field,m_Fields[Field].ColWidth);
	}
	WriteTrace(TraceDebug,"SaveRomListColoumnInfo - End");
}

int CALLBACK CRomBrowser::SelectRomDirCallBack(WND_HANDLE hwnd,DWORD uMsg,DWORD lp, DWORD lpData) {
  switch(uMsg)
  {
    case BFFM_INITIALIZED:
      // WParam is TRUE since you are passing a path.
      // It would be FALSE if you were passing a pidl.
      if (lpData)
      {
        SendMessage((HWND)hwnd,BFFM_SETSELECTION,TRUE,lpData);
      }
      break;
  } 
  return 0;
}

void CRomBrowser::SelectRomDir(CNotification * Notify) {
	char SelectedDir[MAX_PATH];
	LPITEMIDLIST pidl;
	BROWSEINFO bi;

	WriteTrace(TraceDebug,"CRomBrowser::SelectRomDir 1");
	stdstr RomDir = _Settings->LoadString(RomDirectory);
	bi.hwndOwner = (HWND)m_MainWindow;
	bi.pidlRoot = NULL;
	bi.pszDisplayName = SelectedDir;
	bi.lpszTitle = GS(SELECT_ROM_DIR);
	bi.ulFlags = BIF_RETURNFSANCESTORS | BIF_RETURNONLYFSDIRS;
	bi.lpfn = (BFFCALLBACK)SelectRomDirCallBack;
	bi.lParam = (DWORD)RomDir.c_str();
	WriteTrace(TraceDebug,"CRomBrowser::SelectRomDir 2");
	if ((pidl = SHBrowseForFolder(&bi)) != NULL) {
		WriteTrace(TraceDebug,"CRomBrowser::SelectRomDir 3");
		char Directory[_MAX_PATH];
		if (SHGetPathFromIDList(pidl, Directory)) {
			int len = strlen(Directory);

			WriteTrace(TraceDebug,"CRomBrowser::SelectRomDir 4");
			if (Directory[len - 1] != '\\') {
				strcat(Directory,"\\");
			}
			WriteTrace(TraceDebug,"CRomBrowser::SelectRomDir 4");
			_Settings->SaveDword(UseRomDirSelected,true);
			WriteTrace(TraceDebug,"CRomBrowser::SelectRomDir 5");
			_Settings->SaveString(SelectedRomDir,Directory);
			WriteTrace(TraceDebug,"CRomBrowser::SelectRomDir 6");
			_Settings->SaveString(RomDirectory,Directory);
			WriteTrace(TraceDebug,"CRomBrowser::SelectRomDir 7");
			Notify->AddRecentDir(Directory);
			WriteTrace(TraceDebug,"CRomBrowser::SelectRomDir 8");
			RefreshRomBrowser();
			WriteTrace(TraceDebug,"CRomBrowser::SelectRomDir 9");
		}
	}
}

void CRomBrowser::FixRomListWindow (void) {
	//Change the window Style
	long Style = GetWindowLong((HWND)m_MainWindow,GWL_STYLE) | WS_SIZEBOX | WS_MAXIMIZEBOX;
	SetWindowLong((HWND)m_MainWindow,GWL_STYLE,Style);
	
	//Fix height and width
	int Width  = _Settings->LoadDword(RomBrowserWidth);
	int Height = _Settings->LoadDword(RomBrowserHeight);

	if (Width < 200)  { Width = 200;  }
	if (Height < 200) { Height = 200; }

	RECT rcClient,rcWindow;
	GetClientRect((HWND)m_MainWindow,&rcClient);
	GetWindowRect((HWND)m_MainWindow,&rcWindow);

	//fix up for the window borders
	rcWindow.top    -= GetSystemMetrics(SM_CXBORDER); rcWindow.left   -= GetSystemMetrics(SM_CXBORDER);
	rcWindow.bottom += GetSystemMetrics(SM_CXBORDER); rcWindow.right  += GetSystemMetrics(SM_CXBORDER);

	SetRect(&rcClient,0,0,Width + ((rcWindow.right - rcWindow.left) - rcClient.right),
		Height + ((rcWindow.bottom - rcWindow.top) - rcClient.bottom));

	//Fix window location
	DWORD Left = (GetSystemMetrics( SM_CXSCREEN ) - rcClient.right) / 2;
	DWORD Top = (GetSystemMetrics( SM_CYSCREEN ) - rcClient.bottom) / 2;
	_Settings->LoadDword(RomBrowserTop, Top);
	_Settings->LoadDword(RomBrowserLeft,Left);

    //Change the window to the correct location and dimensions
	MoveWindow( (HWND)m_MainWindow, Left, Top, rcClient.right - rcClient.left, 
		rcClient.bottom - rcClient.top, TRUE );

	//Make the screen maximized if it was
	if (_Settings->LoadDword(RomBrowserMaximized)) {
		ShowWindow((HWND)m_MainWindow,SW_MAXIMIZE); 
	}

}

void CRomBrowser::ShowRomList (void) {
	if (_Settings->LoadBool(CPU_Running)) { return; }
	m_ShowingRomBrowser = true;
	WatchThreadStop();
	FixRomListWindow();
	if (m_hRomList == NULL) { CreateRomListControl(); }	
	EnableWindow((HWND)m_hRomList,TRUE);
	ShowWindow((HWND)m_hRomList,SW_SHOW);
	m_AllowSelectionLastRom = true;

	//Make sure selected item is visible
	int iItem = ListView_GetNextItem((HWND)m_hRomList, -1, LVNI_SELECTED);
	ListView_EnsureVisible((HWND)m_hRomList,iItem,FALSE);

	//Mark the window as visible
	m_Visible = true;

	RECT rcWindow;
	if (GetClientRect((HWND)m_MainWindow,&rcWindow))
	{
		ResizeRomList(rcWindow.right,rcWindow.bottom);
	}

	InvalidateRect((HWND)m_hRomList,NULL,TRUE);

	//Start thread to watch for dir changed
	WatchThreadStart();
	m_ShowingRomBrowser = false;
}

void CRomBrowser::HideRomList (void) {
	if (!RomBrowserVisible()) { return; }
	ShowWindow((HWND)m_MainWindow,SW_HIDE);

	SaveRomListColoumnInfo();
	WatchThreadStop();
	
	//Make sure the window does disappear
	Sleep(100);
	
	//Disable the rom list
	EnableWindow((HWND)m_hRomList,FALSE);
	ShowWindow((HWND)m_hRomList,SW_HIDE);

	if (_Settings->LoadDword(RomBrowserMaximized)) { ShowWindow((HWND)m_MainWindow,SW_RESTORE); }

	//Change the window style
	long Style = GetWindowLong((HWND)m_MainWindow,GWL_STYLE) &	~(WS_SIZEBOX | WS_MAXIMIZEBOX);
	SetWindowLong((HWND)m_MainWindow,GWL_STYLE,Style);

	//Move window to correct location
	RECT rect;
	GetWindowRect((HWND)m_MainWindow,&rect);
	int X = (GetSystemMetrics( SM_CXSCREEN ) - (rect.right - rect.left)) / 2;
	int	Y = (GetSystemMetrics( SM_CYSCREEN ) - (rect.bottom - rect.top)) / 2;
	_Settings->LoadDword(MainWindowTop,(DWORD &)Y);
	_Settings->LoadDword(MainWindowLeft,(DWORD &)X);
	SetWindowPos((HWND)m_MainWindow,NULL,X,Y,0,0,SWP_NOZORDER|SWP_NOSIZE);

	//Mark the window as not visible
	m_Visible = false;

	//Make the main window visible again
	ShowWindow((HWND)m_MainWindow,SW_SHOW);
	BringWindowToTop((HWND)m_MainWindow);
	PostMessage((HWND)m_MainWindow, WM_USER + 17, 0,0 );

}

bool CRomBrowser::RomDirNeedsRefresh ( void )
{
	bool InWatchThread = (m_WatchThreadID ==  GetCurrentThreadId());
	
	//Get Old MD5 of file names
	stdstr FileName = _Settings->LoadString(RomListCache);
	HANDLE hFile = CreateFile(FileName.c_str(),GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
	if (hFile == INVALID_HANDLE_VALUE) 
	{
		//if file does not exist then refresh the data
		return true;
	}
	
	DWORD dwRead;
	unsigned char CurrentFileMD5[16];
	ReadFile(hFile,&CurrentFileMD5,sizeof(CurrentFileMD5),&dwRead,NULL);
	CloseHandle(hFile);

	//Get Current MD5 of file names
	strlist FileNames;
	GetRomFileNames(FileNames,CPath(_Settings->LoadString(RomDirectory)),stdstr(""), InWatchThread );
	FileNames.sort();
	
	MD5 NewMd5 = RomListHash(FileNames);
	if (memcmp(NewMd5.raw_digest(),CurrentFileMD5,sizeof(CurrentFileMD5)) != 0)
	{
		return true;
	}
	return false;
}

MD5 CRomBrowser::RomListHash ( strlist & FileList )
{
	stdstr NewFileNames;
	for (strlist::iterator iter = FileList.begin(); iter != FileList.end(); iter++)
	{
		NewFileNames += *iter;
		NewFileNames += ";";
	}
	MD5 md5Hash((const unsigned char *)NewFileNames.c_str(), NewFileNames.length());
	WriteTraceF(TraceDebug,"RomListHash: %s - %s",md5Hash.hex_digest(),NewFileNames.c_str());
	return md5Hash;
}

void CRomBrowser::WatchRomDirChanged ( CRomBrowser * _this )
{
	try
	{
		WriteTrace(TraceDebug,"CRomBrowser::WatchRomDirChanged 1");
		_this->m_WatchRomDir = _Settings->LoadString(RomDirectory);
		WriteTrace(TraceDebug,"CRomBrowser::WatchRomDirChanged 2");
		if (_this->RomDirNeedsRefresh())
		{
			WriteTrace(TraceDebug,"CRomBrowser::WatchRomDirChanged 2a");
			PostMessage((HWND)_this->m_MainWindow,WM_COMMAND,ID_FILE_REFRESHROMLIST,0);
		}
		WriteTrace(TraceDebug,"CRomBrowser::WatchRomDirChanged 3");
		HANDLE hChange[] = {
			_this->m_WatchStopEvent,
			FindFirstChangeNotification(_this->m_WatchRomDir.c_str(),_Settings->LoadDword(RomBrowserRecursive),FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE),
		};
		WriteTrace(TraceDebug,"CRomBrowser::WatchRomDirChanged 4");
		for (;;)
		{
			WriteTrace(TraceDebug,"CRomBrowser::WatchRomDirChanged 5");
			if (WaitForMultipleObjects(sizeof(hChange) / sizeof(hChange[0]),hChange,false,INFINITE) == WAIT_OBJECT_0)
			{
				WriteTrace(TraceDebug,"CRomBrowser::WatchRomDirChanged 5a");
				FindCloseChangeNotification(hChange[1]);
				return;
			}
			WriteTrace(TraceDebug,"CRomBrowser::WatchRomDirChanged 5b");
			if (_this->RomDirNeedsRefresh())
			{
				PostMessage((HWND)_this->m_MainWindow,WM_COMMAND,ID_FILE_REFRESHROMLIST,0);
			}
			WriteTrace(TraceDebug,"CRomBrowser::WatchRomDirChanged 5c");
			if (!FindNextChangeNotification(hChange[1]))
			{
				FindCloseChangeNotification(hChange[1]);
				return;
			}
			WriteTrace(TraceDebug,"CRomBrowser::WatchRomDirChanged 5d");
		}
		WriteTrace(TraceDebug,"CRomBrowser::WatchRomDirChanged 5e");
	}
	catch (...)
	{
		WriteTraceF(TraceError,_T("CRomBrowser::WatchRomDirChanged(): Unhandled Exception"));
	}
}

void CRomBrowser::WatchThreadStart (void)
{
	WriteTrace(TraceDebug,"CRomBrowser::WatchThreadStart 1");
	WatchThreadStop();
	WriteTrace(TraceDebug,"CRomBrowser::WatchThreadStart 2");
	m_WatchStopEvent = CreateEvent(NULL,true,false,NULL);
	WriteTrace(TraceDebug,"CRomBrowser::WatchThreadStart 3");
	m_WatchThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)WatchRomDirChanged,this,0,&m_WatchThreadID);
	WriteTrace(TraceDebug,"CRomBrowser::WatchThreadStart 4");
}

void CRomBrowser::WatchThreadStop( void )
{
	if (m_WatchThread == NULL)
	{
		return;
	}
	WriteTrace(TraceDebug,"CRomBrowser::WatchThreadStop 1");
	SetEvent(m_WatchStopEvent);
	DWORD ExitCode;
	for (int count = 0; count < 20; count ++ ) {
	WriteTrace(TraceDebug,"CRomBrowser::WatchThreadStop 2");
		GetExitCodeThread(m_WatchThread,&ExitCode);
		if (ExitCode != STILL_ACTIVE) {
			break;
		}
		Sleep(200);
	}
	WriteTrace(TraceDebug,"CRomBrowser::WatchThreadStop 3");
	if (ExitCode == STILL_ACTIVE) 
	{
		WriteTrace(TraceDebug,"CRomBrowser::WatchThreadStop 3a");
		TerminateThread(m_WatchThread,0); 
	}
	WriteTrace(TraceDebug,"CRomBrowser::WatchThreadStop 4");

	CloseHandle(m_WatchThread);
	CloseHandle(m_WatchStopEvent);
	m_WatchStopEvent = NULL;
	m_WatchThread    = NULL;
	m_WatchThreadID  = 0;
	WriteTrace(TraceDebug,"CRomBrowser::WatchThreadStop 5");

}

void CRomBrowser::Store7ZipInfo (CSettings * Settings, C7zip & ZipFile, int FileNo )
{
	// if we have infomation about this file already then leave this function
	CIniFile zipInfo(Settings->LoadString(ZipCacheIniName).c_str());
	CFileItem * cf = ZipFile.FileItem(FileNo);

	char FileName[260];
	stdstr_f SectionName("%s-%d",ZipFile.FileName(FileName,sizeof(FileName)),ZipFile.FileSize());
	SectionName.ToLower();

	char Header[0x80];
	if (zipInfo.GetString(SectionName.c_str(),cf->Name,"",Header,sizeof(Header)) > 0)
	{
		return;
	}

	// Gather all information that is in relation to this file
	for (int i = 0; i < ZipFile.NumFiles(); i++)
	{
		CFileItem * f = ZipFile.FileItem(i);

		BYTE RomData[0x1000];
		ZipFile.GetFile(i,RomData,sizeof(RomData));
		
		if (!IsValidRomImage(RomData)) { continue; }
		ByteSwapRomData(RomData,sizeof(RomData));
		
		stdstr RomHeader;
		for (int x = 0; x < 0x40; x += 4)
		{
			RomHeader += stdstr_f("%08X",*((DWORD *)&RomData[x]));
		}
		int CicChip = GetCicChipID(RomData);

		//save this info
		zipInfo.SaveString(SectionName.c_str(),f->Name,RomHeader.c_str());
		zipInfo.SaveNumber(SectionName.c_str(),stdstr_f("%s-Cic",f->Name).c_str(),CicChip);

	}
	
	//delete cache
	stdstr CacheFileName = Settings->LoadString(RomListCache);
	DeleteFile(CacheFileName.c_str());
}

void CRomBrowser::SetPluginList ( CPlugins * Plugins )
{
	m_Plugins = Plugins;
}
