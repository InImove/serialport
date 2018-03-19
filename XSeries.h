#pragma once
#include <Windows.h>

#define  READ_THREAD_BUFSIZE 4096
#define  WRITETHERAD_COM_PORT L"WRITETHERAD_COM_PORT"  //��Ϣ������

// �ļ��ڴ�:ʵ�ֽ��̼����ݹ���(ע���ǹ̶����ݿ�����ݹ���) 
class CFileMemory
{
public:
	CFileMemory();
	virtual ~CFileMemory();

public:
	virtual BOOL Create(const TCHAR* pszName, int iSize, DWORD dwCreationDisposition, BOOL& bFirstCreate);
	virtual void Close();
	BOOL Lock();
	void Unlock();
	virtual DWORD  Read(BYTE * pBuf, DWORD dwBufLen);
	virtual DWORD  Write(BYTE * pBuf, DWORD dwBufLen);
	BYTE  * GetBuffer();

protected:
	HANDLE	m_hMutex;		   // ����ͬ�����ʹ����ڴ滺�����Ļ���
	HANDLE	m_hFileMapping;	   // �����ڴ�ӳ����
	BYTE  *	m_pBuffer;		   // ָ�����ڴ滺������ָ��
	DWORD	m_dwBufferLength;  // �������ĳ���
};


// ���̼����ݿ鹲�� ,����ͬʱһ��һд                                                                                                     
class CFileMemoryPipe : public CFileMemory
{
public:
	CFileMemoryPipe();
	virtual ~CFileMemoryPipe();

public:
	virtual BOOL Create(const TCHAR* pszName, int iSize, DWORD dwCreationDisposition, BOOL& bFirstCreate);
	virtual void Close();
	virtual DWORD  Read(BYTE * pBuf, DWORD dwBufLen);
	virtual DWORD  Write(BYTE * pBuf, DWORD dwBufLen);

protected:
	HANDLE	m_hReadableEvent;	 // �ֶ������¼�����֪ͨ�ܵ�����һ�˿ɶ�ȡ���ݡ�
	HANDLE	m_hWriteableEvent;   // �ֶ������¼�����֪ͨ�ܵ�����һ�˿�д�����ݡ�
};

/*! @class
********************************************************************************
������   : CFmpRPoint
����     : �ӽ��̼�Ĺ������ݿ��ж����ݣ���CFmpWPoint���ʹ��
�쳣��   : 
--------------------------------------------------------------------------------
��ע     : ����close�ӿڣ�ֻ���ÿɶ��¼�
�����÷� : 
--------------------------------------------------------------------------------
����     : 
����ʱ�� : 
--------------------------------------------------------------------------------
�޸ļ�¼ : 
�� ��        �汾 �޸���       �޸�����  
*******************************************************************************/
class CFmpRPoint : public CFileMemoryPipe
{
public:
	CFmpRPoint(){return;};
	~CFmpRPoint(){return;};  

public:
	virtual void Close();

private:
	DWORD  Write(BYTE * pBuf, DWORD dwBufLen) {return 0;}; // ���ṩ��
};

/*! @class
********************************************************************************
������   : CFmpWPoint
����     : ����̼�Ĺ������ݿ���д���ݣ���CFmpRPoint���ʹ��
�쳣��   : 
--------------------------------------------------------------------------------
��ע     : ����close�ӿڣ�ֻ���ÿ�д�¼�
�����÷� : 
--------------------------------------------------------------------------------
����     : 
����ʱ�� : 
--------------------------------------------------------------------------------
�޸ļ�¼ : 
�� ��        �汾 �޸���       �޸�����    
*******************************************************************************/
class CFmpWPoint : public CFileMemoryPipe
{
public:
	CFmpWPoint() {return;};
	~CFmpWPoint() {return;}; 

public:
	virtual void Close();

private:
	DWORD  Read(BYTE * pBuf, DWORD dwBufLen) {return 0;};  // ���ṩ��
};


/*! @class
********************************************************************************
������   : IPortOwer
����     : ���뱻 CXSeries ���ӵ�߼̳�
           
�쳣��   : 
--------------------------------------------------------------------------------
��ע     : 
�����÷� : 
--------------------------------------------------------------------------------
����     : <hrg>  
����ʱ�� : 2018/03/15 
--------------------------------------------------------------------------------
�޸ļ�¼ : 
�� ��        �汾 �޸���       �޸����� 

*******************************************************************************/
class IPortOwer  
{
public:
    IPortOwer(){};
    virtual ~IPortOwer(){};
    
///////////////////////////////////////////////////
//��Ա����
public:
    //��ʼ����Ϣ��WM_INIT��
    virtual void OnSeriesRead(BYTE *pbyBuf, DWORD dwBufLen, DWORD *pActualHandle) = 0;
	//virtual void OnSeriesWrite(BYTE *pbyBuf, DWORD dwBufLen, DWORD *pActualHandle) = 0;

};




/*! @class
********************************************************************************
������   : CXSeries
����     : ���ڶ�дͨ��
�쳣��   : 
--------------------------------------------------------------------------------
��ע     : ��֧����̵� OwerWritePort
�����÷� : 
--------------------------------------------------------------------------------
����     : <hrg>  
����ʱ�� : 2018/03/15
--------------------------------------------------------------------------------
�޸ļ�¼ : 
�� ��        �汾 �޸���       �޸����� 
*******************************************************************************/
class CXSeries
{
public:
	CXSeries(void);
	~CXSeries(void);
	virtual DWORD OPenSeries(IPortOwer* pPortOwer, //�������ӵ����
							UINT port,            //���ں�
							UINT baud ,           //������
							UINT parity,          //��żУ��λ
							UINT databits,        //����λ
							UINT stopbits         //ֹͣλ
							);
	void ClosePort();
	DWORD WThreadProc();
	DWORD RThreadProc();
	BOOL WritePort(HANDLE hComm, const BYTE *pbyBuf, DWORD dwBufLen);
	//BOOL WritePort(HANDLE hComm, const BYTE *pbyBuf, DWORD dwBufLen)
	static DWORD CALLBACK WThread(LPVOID pParam);
	static DWORD CALLBACK RThread(LPVOID pParam);
private:
	HANDLE m_hCom;			    // �Ѵ򿪵Ĵ��ھ��
	IPortOwer * m_pProtOwer;    // ����ӵ����
	BYTE * m_pbyReadBuf;        // ���߳�buffer
	// ��ʼ���߳̽�����־ 
	bool m_fRThreadExit;		// ���߳̽�����־
	bool m_fWThreadExit;		// д�߳̽�����־
	HANDLE m_hRthread;			// ���߳̾��
	HANDLE m_hWthread;			// д�߳̾��
	DWORD  m_hRthreadID;		// ���߳�ID��ʶ
	DWORD  m_hWthreadID;		// д�߳�ID��ʶ
	CFmpWPoint m_objWCpu2McuPipe;
	CFmpRPoint m_objRCpu2McuPipe;

	OVERLAPPED  m_olWrite;      // �첽���������Ϣ�Ľṹ��
	OVERLAPPED	m_olWaite;
	OVERLAPPED	m_olRead;
};
