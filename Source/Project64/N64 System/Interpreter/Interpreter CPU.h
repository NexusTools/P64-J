class CInterpreterCPU :
	private R4300iOp
{
public:
	 CInterpreterCPU();
	~CInterpreterCPU();

	void StartInterpreterCPU (void );

	R4300iOp::Func * m_R4300i_Opcode;

private:
};