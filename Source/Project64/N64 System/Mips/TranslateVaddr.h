class CTransVaddr
{
public:
	virtual bool TranslateVaddr ( DWORD VAddr, DWORD &PAddr) const  = 0;
	virtual bool ValidVaddr  ( DWORD VAddr ) const = 0;
};