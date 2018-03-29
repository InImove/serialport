#pragma once
#include <Windows.h>

#define  READ_THREAD_BUFSIZE 4096
#define  WRITETHERAD_COM_PORT L"WRITETHERAD_COM_PORT"  //消息队列名

// 文件内存:实现进程间数据共享(注意是固定数据块的数据共享) 
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
	HANDLE	m_hMutex;		   // 用于同步访问共享内存缓冲区的互斥
	HANDLE	m_hFileMapping;	   // 共享内存映射句柄
	BYTE  *	m_pBuffer;		   // 指向共享内存缓冲区的指针
	DWORD	m_dwBufferLength;  // 缓冲区的长度
};


// 进程间数据块共享 ,可以同时一读一写                                                                                                     
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
	HANDLE	m_hReadableEvent;	 // 手动重置事件，以通知管道的另一端可读取数据。
	HANDLE	m_hWriteableEvent;   // 手动重置事件，以通知管道的另一端可写入数据。
};

/*! @class
********************************************************************************
类名称   : CFmpRPoint
功能     : 从进程间的共享数据块中读数据，与CFmpWPoint配合使用
异常类   : 
--------------------------------------------------------------------------------
备注     : 重载close接口，只设置可读事件
典型用法 : 
--------------------------------------------------------------------------------
作者     : 
创建时间 : 
--------------------------------------------------------------------------------
修改记录 : 
日 期        版本 修改人       修改内容  
*******************************************************************************/
class CFmpRPoint : public CFileMemoryPipe
{
public:
	CFmpRPoint(){return;};
	~CFmpRPoint(){return;};  

public:
	virtual void Close();

private:
	//DWORD  Write(BYTE * pBuf, DWORD dwBufLen) {return 0;}; // 不提供读
};

/*! @class
********************************************************************************
类名称   : CFmpWPoint
功能     : 向进程间的共享数据块中写数据，与CFmpRPoint配合使用
异常类   : 
--------------------------------------------------------------------------------
备注     : 重载close接口，只设置可写事件
典型用法 : 
--------------------------------------------------------------------------------
作者     : 
创建时间 : 
--------------------------------------------------------------------------------
修改记录 : 
日 期        版本 修改人       修改内容    
*******************************************************************************/
class CFmpWPoint : public CFileMemoryPipe
{
public:
	CFmpWPoint() {return;};
	~CFmpWPoint() {return;}; 

public:
	virtual void Close();

private:
//	DWORD  Read(BYTE * pBuf, DWORD dwBufLen) {return 0;};  // 不提供读
};


/*! @class
********************************************************************************
类名称   : IPortOwer
功能     : 必须被 CXSeries 类的拥者继承
           
异常类   : 
--------------------------------------------------------------------------------
备注     : 
典型用法 : 
--------------------------------------------------------------------------------
作者     : <hrg>  
创建时间 : 2018/03/15 
--------------------------------------------------------------------------------
修改记录 : 
日 期        版本 修改人       修改内容 

*******************************************************************************/
class IPortOwer  
{
public:
    IPortOwer(){};
    virtual ~IPortOwer(){};
    
///////////////////////////////////////////////////
//成员函数
public:
    //初始化消息（WM_INIT）
    virtual void OnSeriesRead(BYTE *pbyBuf, DWORD dwBufLen, DWORD *pActualHandle) = 0;
	//virtual void OnSeriesWrite(BYTE *pbyBuf, DWORD dwBufLen, DWORD *pActualHandle) = 0;

};




/*! @class
********************************************************************************
类名称   : CXSeries
功能     : 串口读写通信
异常类   : 
--------------------------------------------------------------------------------
备注     : 不支跨进程的 OwerWritePort
典型用法 : 
--------------------------------------------------------------------------------
作者     : <hrg>  
创建时间 : 2018/03/15
--------------------------------------------------------------------------------
修改记录 : 
日 期        版本 修改人       修改内容 
*******************************************************************************/
class CXSeries
{
public:
	CXSeries(void);
	~CXSeries(void);
	virtual DWORD OPenSeries(IPortOwer* pPortOwer, //串口类的拥有者
							UINT port,            //串口号
							UINT baud ,           //波特率
							UINT parity,          //奇偶校验位
							UINT databits,        //数据位
							UINT stopbits         //停止位
							);
	void ClosePort();
	DWORD WThreadProc();
	DWORD RThreadProc(LPVOID pParam);
	BOOL WritePort(HANDLE hComm, const BYTE *pbyBuf, DWORD dwBufLen);
	BOOL OwerWritePort(const BYTE * pbyBuf, DWORD dwBufLen);
	//BOOL WritePort(HANDLE hComm, const BYTE *pbyBuf, DWORD dwBufLen)
	static DWORD CALLBACK WThread(LPVOID pParam);
	static DWORD CALLBACK RThread(LPVOID pParam);
	virtual void CloseWThread();
	virtual void CloseRThread();
private:
	
	IPortOwer * m_pProtOwer;    // 串口拥有者
	BYTE * m_pbyReadBuf;        // 读线程buffer
	// 初始化线程结束标志 
	bool m_fRThreadExit;		// 读线程结束标志
	bool m_fWThreadExit;		// 写线程结束标志
	HANDLE m_hRthread;			// 读线程句柄
	HANDLE m_hWthread;			// 写线程句柄
	DWORD  m_hRthreadID;		// 读线程ID标识
	DWORD  m_hWthreadID;		// 写线程ID标识
	CFmpWPoint m_objWCpu2McuPipe;
	CFmpRPoint m_objRCpu2McuPipe;

	OVERLAPPED  m_olWrite;      // 异步输入输出信息的结构体
	OVERLAPPED	m_olWaite;
	OVERLAPPED	m_olRead;
	HANDLE m_hCom;			    // 已打开的串口句柄
};
