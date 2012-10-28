#include "..\..\Settings.h"
#include "..\..\User Interface.h"
#include "SettingsType-Application.h"

bool       CSettingTypeApplication::m_UseRegistry     = false;
CIniFile * CSettingTypeApplication::m_SettingsIniFile = NULL;

CSettingTypeApplication::CSettingTypeApplication(LPCSTR Section, LPCSTR Name, DWORD DefaultValue ) :
	m_Section(FixSectionName(Section)),
	m_KeyName(Name),
	m_DefaultStr(""),
	m_DefaultValue(DefaultValue),
	m_DefaultSetting(Default_Constant),
	m_KeyNameIdex(m_KeyName)
{
}

CSettingTypeApplication::CSettingTypeApplication(LPCSTR Section, LPCSTR Name, bool DefaultValue ) :
	m_Section(FixSectionName(Section)),
	m_KeyName(Name),
	m_DefaultStr(""),
	m_DefaultValue(DefaultValue),
	m_DefaultSetting(Default_Constant),
	m_KeyNameIdex(m_KeyName)
{
}

CSettingTypeApplication::CSettingTypeApplication(LPCSTR Section, LPCSTR Name, LPCSTR DefaultValue ) :
	m_Section(FixSectionName(Section)),
	m_KeyName(Name),
	m_DefaultStr(DefaultValue),
	m_DefaultValue(0),
	m_DefaultSetting(Default_Constant),
	m_KeyNameIdex(m_KeyName)
{
}

CSettingTypeApplication::CSettingTypeApplication(LPCSTR Section, LPCSTR Name, SettingID DefaultSetting ) :
	m_Section(FixSectionName(Section)),
	m_KeyName(Name),
	m_DefaultStr(""),
	m_DefaultValue(0),
	m_DefaultSetting(DefaultSetting),
	m_KeyNameIdex(m_KeyName)
{
}

CSettingTypeApplication::~CSettingTypeApplication()
{
}


void CSettingTypeApplication::Initilize( const char * AppName )
{
	stdstr SettingsFile, OrigSettingsFile;
	
	for (int i = 0; i < 100; i++)
	{
		OrigSettingsFile = SettingsFile;
		SettingsFile = _Settings->LoadString(SupportFile_Settings);
		if (SettingsFile == OrigSettingsFile)
		{
			break;
		}
		if (m_SettingsIniFile)
		{
			delete m_SettingsIniFile;
		}
		m_SettingsIniFile = new CIniFile(SettingsFile.c_str());
	} while (SettingsFile != OrigSettingsFile);
	
	m_SettingsIniFile->SetAutoFlush(false);
	m_UseRegistry = _Settings->LoadBool(Setting_UseFromRegistry);
}


void CSettingTypeApplication::CleanUp()
{
	if (m_SettingsIniFile)
	{
		m_SettingsIniFile->SetAutoFlush(true);
		delete m_SettingsIniFile;
		m_SettingsIniFile = NULL;
	}
}

bool CSettingTypeApplication::Load ( int Index, bool & Value ) const
{
	bool bRes = false;

	if (!m_UseRegistry)
	{
		DWORD dwValue;
		bRes = m_SettingsIniFile->GetNumber(SectionName(),m_KeyNameIdex.c_str(),Value,dwValue);
		if (bRes)
		{
			Value = dwValue != 0;
		}
	} else {
		Notify().BreakPoint(__FILE__,__LINE__); 
	}
	
	if (!bRes && m_DefaultSetting != Default_None)
	{
		if (m_DefaultSetting == Default_Constant)
		{
			Value = m_DefaultValue != 0;
		} else {
			_Settings->LoadBool(m_DefaultSetting,Value);
		}
	}
	return bRes;
}

bool CSettingTypeApplication::Load ( int Index, ULONG & Value ) const
{
	bool bRes;
	if (!m_UseRegistry)
	{
		bRes = m_SettingsIniFile->GetNumber(SectionName(),m_KeyNameIdex.c_str(),Value,Value);
	} else {
		Notify().BreakPoint(__FILE__,__LINE__); 
	}
	if (!bRes && m_DefaultSetting != Default_None)
	{
		if (m_DefaultSetting == Default_Constant)
		{
			Value = m_DefaultValue;
		} else {
			_Settings->LoadDword(m_DefaultSetting,Value);
		}
	}
	return bRes;
}

LPCSTR CSettingTypeApplication::SectionName ( void ) const 
{
	return m_Section.c_str();
}

bool CSettingTypeApplication::Load ( int Index, stdstr & Value ) const
{
	bool bRes;
	if (!m_UseRegistry)
	{
		if (m_SettingsIniFile)
		{
			bRes = m_SettingsIniFile->GetString(SectionName(),m_KeyNameIdex.c_str(),m_DefaultStr,Value);
		} else {
			bRes = false;
		}
	} else {
		Notify().BreakPoint(__FILE__,__LINE__); 
	}
	if (!bRes)
	{
		CSettingTypeApplication::LoadDefault(Index,Value);
	}
	return bRes;
}

//return the default values
void CSettingTypeApplication::LoadDefault ( int Index, bool & Value   ) const
{
	if (m_DefaultSetting != Default_None)
	{
		if (m_DefaultSetting == Default_Constant)
		{
			Value = m_DefaultValue != 0;
		} else {
			_Settings->LoadBool(m_DefaultSetting,Value);
		}
	}
}

void CSettingTypeApplication::LoadDefault ( int Index, ULONG & Value  ) const
{
	if (m_DefaultSetting != Default_None)
	{
		if (m_DefaultSetting == Default_Constant)
		{
			Value = m_DefaultValue;
		} else {
			_Settings->LoadDword(m_DefaultSetting,Value);
		}
	}
}

void CSettingTypeApplication::LoadDefault ( int Index, stdstr & Value ) const
{
	if (m_DefaultSetting != Default_None)
	{
		if (m_DefaultSetting == Default_Constant)
		{
			Value = m_DefaultStr;
		} else {
			_Settings->LoadString(m_DefaultSetting,Value);
		}
	}
}

//Update the settings
void CSettingTypeApplication::Save ( int Index, bool Value )
{
	if (!m_UseRegistry)
	{
		m_SettingsIniFile->SaveNumber(SectionName(),m_KeyNameIdex.c_str(),Value);
	} else {
		Notify().BreakPoint(__FILE__,__LINE__); 
	}
}

void CSettingTypeApplication::Save ( int Index, ULONG Value )
{
	if (!m_UseRegistry)
	{
		m_SettingsIniFile->SaveNumber(SectionName(),m_KeyNameIdex.c_str(),Value);
	} else {
		Notify().BreakPoint(__FILE__,__LINE__); 
	}
}

void CSettingTypeApplication::Save ( int Index, const stdstr & Value )
{
	if (!m_UseRegistry)
	{
		m_SettingsIniFile->SaveString(SectionName(),m_KeyNameIdex.c_str(),Value.c_str());
	} else {
		Notify().BreakPoint(__FILE__,__LINE__); 
	}
}

void CSettingTypeApplication::Save ( int Index, const char * Value )
{
	if (!m_UseRegistry)
	{
		m_SettingsIniFile->SaveString(SectionName(),m_KeyNameIdex.c_str(),Value);
	} else {
		Notify().BreakPoint(__FILE__,__LINE__); 
	}
}

stdstr CSettingTypeApplication::FixSectionName(LPCSTR Section)
{
	stdstr SectionName(Section);

	if (!m_UseRegistry)
	{
		if (SectionName.empty()) 
		{ 
			SectionName = "default";
		}
		SectionName.replace("\\","-");
	}
	return SectionName;
}

void CSettingTypeApplication::Delete( int Index )
{
	if (!m_UseRegistry)
	{
		m_SettingsIniFile->SaveString(SectionName(),m_KeyNameIdex.c_str(),NULL);
	} else {
		Notify().BreakPoint(__FILE__,__LINE__); 
	}
}
