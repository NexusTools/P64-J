class CMipsMemoryVM :
	public CMipsMemory,
	public CTransVaddr,
	private CRecompilerOps,
	private R4300iOp
{
	CMipsMemory_CallBack * const m_CBClass;

	//Memory Locations
	BYTE          * m_RDRAM, * m_DMEM, * m_IMEM, m_PIF_Ram[0x40];
	DWORD         m_AllocatedRdramSize;

	//Rom Information
	bool          m_RomMapped;
	BYTE *        m_Rom;
	DWORD         m_RomSize;
	bool          m_RomWrittenTo;
	DWORD         m_RomWroteValue;

	//Current Half line
	void UpdateHalfLine       ( void );
	DWORD         m_HalfLine;
	DWORD         m_MemoryStack;
	DWORD         m_TempValue;

	//Initilizing and reseting information about the memory system
	void FreeMemory           ( void );

public:
	       CMipsMemoryVM        ( CMipsMemory_CallBack * CallBack );
	      ~CMipsMemoryVM        ( void );
	
	BOOL   Initialize   ( void );
	
	BYTE * Rdram        ( void );
	DWORD  RdramSize    ( void );
	BYTE * Dmem         ( void );
	BYTE * Imem         ( void );
	BYTE * PifRam       ( void );

	BOOL  LB_VAddr     ( DWORD VAddr, BYTE & Value );
	BOOL  LH_VAddr     ( DWORD VAddr, WORD & Value ); 
	BOOL  LW_VAddr     ( DWORD VAddr, DWORD & Value );
	BOOL  LD_VAddr     ( DWORD VAddr, QWORD & Value );

	BOOL  SB_VAddr     ( DWORD VAddr, BYTE Value );
	BOOL  SH_VAddr     ( DWORD VAddr, WORD Value );
	BOOL  SW_VAddr     ( DWORD VAddr, DWORD Value );
	BOOL  SD_VAddr     ( DWORD VAddr, QWORD Value );

	int   MemoryFilter ( DWORD dwExptCode, void * lpExceptionPointer );
	
	//Protect the Memory from being written to
	void  ProtectMemory    ( DWORD StartVaddr, DWORD EndVaddr );
	void  UnProtectMemory  ( DWORD StartVaddr, DWORD EndVaddr );

	//Compilation Functions
	void ResetMemoryStack    ( void );
	
	void Compile_LB          ( void );
	void Compile_LBU         ( void );
	void Compile_LH          ( void );
	void Compile_LHU         ( void );
	void Compile_LW          ( void );
	void Compile_LWC1        ( void );
	void Compile_LWU         ( void );
	void Compile_LWL         ( void );
	void Compile_LWR         ( void );
	void Compile_LD          ( void );
	void Compile_LDC1        ( void );
	void Compile_LDL         ( void );
	void Compile_LDR         ( void );
	void Compile_SB          ( void );
	void Compile_SH          ( void );
	void Compile_SW          ( void );
	void Compile_SWL         ( void );
	void Compile_SWR         ( void );
	void Compile_SD          ( void );
	void Compile_SDL         ( void );
	void Compile_SDR         ( void );
	void Compile_SWC1        ( void );
	void Compile_SDC1        ( void );

	void ResetMemoryStack    ( CRegInfo	& RegInfo );
	void Compile_LB          ( CX86Ops::x86Reg Reg, DWORD Addr, BOOL SignExtend );
	void Compile_LH          ( CX86Ops::x86Reg Reg, DWORD Addr, BOOL SignExtend );
	void Compile_LW          ( CX86Ops::x86Reg Reg, DWORD Addr );
	void Compile_SB_Const    ( BYTE Value, DWORD Addr );
	void Compile_SB_Register ( CX86Ops::x86Reg Reg, DWORD Addr );
	void Compile_SH_Const    ( WORD Value, DWORD Addr );
	void Compile_SH_Register ( CX86Ops::x86Reg Reg, DWORD Addr );
	void Compile_SW_Const    ( DWORD Value, DWORD Addr );

	void Compile_SW_Register ( CX86Ops::x86Reg Reg, DWORD Addr );
	  
	//Functions for TLB notification
	void TLB_Mapped ( DWORD VAddr, DWORD Len, DWORD PAddr, bool bReadOnly );
	void TLB_Unmaped ( DWORD Vaddr, DWORD Len );
		  
	// CTransVaddr interface
	bool TranslateVaddr ( DWORD VAddr, DWORD &PAddr) const;
	bool ValidVaddr  ( DWORD VAddr ) const;		  
	
	// Labels
	LPCTSTR LabelName      ( DWORD Address ) const;

private:
	static void RdramChanged      ( CMipsMemoryVM * _this );
	static void ChangeSpStatus    ( void );
	static void ChangeMiIntrMask  ( void );

	int  LB_NonMemory         ( DWORD PAddr, DWORD * Value, BOOL SignExtend );
	int  LH_NonMemory         ( DWORD PAddr, DWORD * Value, int SignExtend );
	int  LW_NonMemory         ( DWORD PAddr, DWORD * Value );

	int  SB_NonMemory         ( DWORD PAddr, BYTE Value );
	int  SH_NonMemory         ( DWORD PAddr, WORD Value );
	int  SW_NonMemory         ( DWORD PAddr, DWORD Value );

	mutable char m_strLabelName[100];

	//BIG look up table to quickly translate the tlb to real mem address
	DWORD * m_TLB_ReadMap;
	DWORD * m_TLB_WriteMap;
};
