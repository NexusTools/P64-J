class CFunctionMap
{
protected:
	typedef CCompiledFunc *  PCCompiledFunc;
	typedef PCCompiledFunc * PCCompiledFunc_TABLE;

	CFunctionMap();
	~CFunctionMap();

	bool AllocateMemory ( void );
	void Reset          ( void );

public:
	inline PCCompiledFunc_TABLE * FunctionTable  ( void ) const { return m_FunctionTable; }
	inline PCCompiledFunc       * JumpTable      ( void ) const { return m_JumpTable; }
	inline BYTE                ** DelaySlotTable ( void ) const { return m_DelaySlotTable; }

private:
	void CleanBuffers  ( void );

	PCCompiledFunc       * m_JumpTable;
	PCCompiledFunc_TABLE * m_FunctionTable;
	BYTE                ** m_DelaySlotTable;
};
