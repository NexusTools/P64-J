#include "../../User Interface.h"
#include "../../User Interface/WTL Controls/ModifiedCheckBox.h"
#include "../../User Interface/WTL Controls/ModifiedEditBox.h"
#include "../../User Interface/WTL Controls/ModifiedComboBox.h"
#include "../../User Interface/WTL Controls/PartialGroupBox.h"
#include "../../User Interface/Settings Config.h"
#include "Settings Page.h"

CConfigSettingSection::CConfigSettingSection( LPCSTR PageTitle ) :
	m_PageTitle(PageTitle)
{
}

CConfigSettingSection::~CConfigSettingSection ()
{
	for (size_t i = 0; i < m_Pages.size(); i++)
	{
		CSettingsPage * Page = m_Pages[i];
		delete Page;
	}
	m_Pages.clear();
}

void CConfigSettingSection::AddPage(CSettingsPage * Page )
{
	m_Pages.push_back(Page);
}

CSettingsPage * CConfigSettingSection::GetPage ( int PageNo )
{
	if (PageNo < 0 || PageNo >= (int)m_Pages.size())
	{
		return NULL;
	}
	return m_Pages[PageNo];
}
