#include "stdafx.h"
#include "XSeries.h"
 #include <assert.h>
#include <WinDef.h>
HANDLE m_hCom = 0;			    // 已打开的串口句柄
/**************************************************************************************************************************************
*                                                                                                                                     *
* Class CFileMemory                                                                                                                   *  
* 进程间固定大小数据块共享                                                                                                            *  
*                                                                                                                                     *  
*                                                                                                                                     *  
**************************************************************************************************************************************/
CFileMemory::CFileMemory()
{
	m_hMutex = NULL;
	m_hFileMapping = NULL;
	m_pBuffer = NULL;
	m_dwBufferLength = 0;
}

CFileMemory::~CFileMemory()
{
	Close();
}


BOOL CFileMemory::Create(const TCHAR* pszName, int iSize, DWORD dwCreationDisposition, BOOL& bFirstCreate)
{
	if (dwCreationDisposition != CREATE_NEW && dwCreationDisposition != OPEN_EXISTING)
		return FALSE;

	BOOL  fResult = FALSE;
	bFirstCreate = FALSE;
	m_dwBufferLength = iSize;

	TCHAR szMutexName[100];
	_stprintf(szMutexName, TEXT("%s.mutex"), pszName);
	m_hMutex = CreateMutex(NULL, FALSE, szMutexName);   // 创建互斥量
	if (NULL == m_hMutex)
	{
		goto cleanup;
	}
	else if (GetLastError() == ERROR_ALREADY_EXISTS)
	{	
	}
	else
	{
		bFirstCreate = TRUE;
	}

	TCHAR szMemName[100];
	_stprintf(szMemName, TEXT("%s.mem"), pszName);
	m_hFileMapping = CreateFileMapping((HANDLE)INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, m_dwBufferLength, szMemName); // 创建共享内存
	if (m_hFileMapping == NULL)
	{
		goto cleanup;
	}

	m_pBuffer = (BYTE*)MapViewOfFile(m_hFileMapping, FILE_MAP_WRITE, 0, 0, 0);  // 获取映射视图文件的开始的地址
	if (NULL == m_pBuffer)
		goto cleanup;

	if (bFirstCreate)
	{
		memset(m_pBuffer, 0, m_dwBufferLength); // 注意,如果是共享的话就不能清空了
	}

	fResult = TRUE;

cleanup:

	if (fResult == FALSE)
	{
		Close();
	}

	return fResult;
}

BOOL CFileMemory::Lock()
{
	return (WAIT_OBJECT_0 == WaitForSingleObject(m_hMutex, INFINITE));
}

void CFileMemory::Unlock()
{
	ReleaseMutex(m_hMutex);
}

void CFileMemory::Close()
{
	if (m_hMutex != NULL)
	{
		CloseHandle(m_hMutex);
		m_hMutex = NULL;
	}

	if (m_pBuffer != NULL)
	{
		UnmapViewOfFile(m_pBuffer);
		m_pBuffer = NULL;
	}

	if (m_hFileMapping != NULL)
	{
		//UnmapViewOfFile(m_hFileMapping);
		CloseHandle(m_hFileMapping);
		m_hFileMapping = NULL;
	}

	m_dwBufferLength = 0;
}


DWORD CFileMemory::Read(BYTE * pBuf, DWORD dwBufLen)
{
	int iBytesRead = 0;

	if ((m_pBuffer == NULL) || (pBuf == NULL))
	{
		return iBytesRead;
	}

	if (Lock())
	{
		// copy data
		iBytesRead = dwBufLen >= m_dwBufferLength ? m_dwBufferLength : dwBufLen;
		memcpy(pBuf, m_pBuffer, iBytesRead);
		Unlock();
	}

	return iBytesRead;
}

DWORD CFileMemory::Write(BYTE * pBuf, DWORD dwBufLen)
{
	int	iBytesWritten = 0;

	if ((m_pBuffer == NULL) || (pBuf == NULL))
	{
		return iBytesWritten;
	}

	if (Lock())
	{
		// copy data
		iBytesWritten = dwBufLen >= m_dwBufferLength ? m_dwBufferLength : dwBufLen;
		memcpy(m_pBuffer, pBuf, iBytesWritten);
		Unlock();
	}

	return iBytesWritten;
}
/**************************************************************************************************************************************
*                                                                                                                                     *
* Class CFileMemoryPipe                                                                                                               *  
* 进程间数据块共享                                                                                                                    *  
* 可以同时一读一写                                                                                                                    *  
*                                                                                                                                     *  
**************************************************************************************************************************************/
CFileMemoryPipe::CFileMemoryPipe()
{
	m_hReadableEvent = NULL;
	m_hWriteableEvent = NULL;
}

CFileMemoryPipe::~CFileMemoryPipe()
{
	Close();
}

BOOL CFileMemoryPipe::Create(const TCHAR* pszName, int iSize, DWORD dwCreationDisposition, BOOL& bFirstCreate)
{
	if (dwCreationDisposition != CREATE_NEW && dwCreationDisposition != OPEN_EXISTING)
		return FALSE;

	BOOL  fResult = FALSE;
	bFirstCreate = FALSE;
	TCHAR szReadableEventName[100];

	_stprintf(szReadableEventName, TEXT("%s.readable"), pszName);
	m_hReadableEvent = CreateEvent(NULL, TRUE, FALSE, szReadableEventName);
	if (m_hReadableEvent == NULL)
	{
		goto cleanup;
	}

	TCHAR szWriteableEventName[100];
	_stprintf(szWriteableEventName, TEXT("%s.writeable"), pszName);
	m_hWriteableEvent = CreateEvent(NULL, TRUE, TRUE, szWriteableEventName);
	if (m_hWriteableEvent == NULL)
	{
		goto cleanup;
	}
	
	fResult = CFileMemory::Create(pszName, iSize, dwCreationDisposition, bFirstCreate);

cleanup:

	if (fResult == FALSE)
	{
		Close();
	}

	return fResult;
}

void CFileMemoryPipe::Close()
{
	if (m_hReadableEvent != NULL)
	{
		// 通知 Read() 里的 WaitForSingleObject(m_hReadableEvent, INFINITE) 退出
		SetEvent(m_hReadableEvent);
		CloseHandle(m_hReadableEvent);
		m_hReadableEvent = NULL;
	}

	if (m_hWriteableEvent != NULL)
	{
		// 通知 Write() 里的 WaitForSingleObject(m_hReadableEvent, INFINITE) 退出
		SetEvent(m_hWriteableEvent);
		CloseHandle(m_hWriteableEvent);
		m_hWriteableEvent = NULL;
	}

	CFileMemory::Close();
}

DWORD CFileMemoryPipe::Read(BYTE * pBuf, DWORD dwBufLen)
{
	DWORD iBytesRead = 0;

	if ((m_pBuffer == NULL) || (pBuf == NULL))
	{
		return iBytesRead;
	}

	// wait for pipe to become readable
	// NOTE: what if the caller already did this, such as the "select" loop in cerunner???
	//       this is a waste and unnecessary penalty then
	//WaitForSingleObject(m_hReadableEvent, INFINITE);
	// serialise access through mutex (timeout after 5 seconds)
	DWORD dwWaitResult = WaitForSingleObject(m_hMutex, 5000);
	if (dwWaitResult == WAIT_FAILED || dwWaitResult == WAIT_TIMEOUT)
	{
		return 0;
	}

	// get length from shared memory
	DWORD  dwLength = ((DWORD*)m_pBuffer)[0];									// 先取得存有数据的长度
	DWORD  dwMaxLength = m_dwBufferLength - sizeof(DWORD);						// 最大数据存量
	bool   fFull = (dwLength >= dwMaxLength);									// 数据是否满了

	//if (0 == dwLength)
	//{
	// 收到的通知，是为了让 Read 退出等待
	//	DebugBreak();
	//}

	// copy data
	if (dwBufLen >= dwLength)													// 如果要读取的数据大于现在有的数据
	{
		iBytesRead = dwLength;													// 可读数据长度			
		memcpy(pBuf, m_pBuffer + sizeof(DWORD), iBytesRead);					// 读取数据
		((DWORD*)m_pBuffer)[0] = 0;												// 把数据长度设为0 
		ResetEvent(m_hReadableEvent);		// no data left for reading			// 释放读信号 
		if (fFull)                           // 读之前是 full,现在读后全为空,通知可写
		{
			SetEvent(m_hWriteableEvent);	// now writeable (only set if was full)
		}
	}
	else																		// 否则读取的数据量小于现在有的数据量	
	{
		iBytesRead = dwBufLen;
		memcpy(pBuf, m_pBuffer + sizeof(DWORD), iBytesRead);					
		dwLength -= iBytesRead;													// 读取不完			
		((DWORD*)m_pBuffer)[0] = dwLength;										// 重新计算长度
		memmove(m_pBuffer + sizeof(DWORD), m_pBuffer + sizeof(DWORD) + iBytesRead, dwLength);
		if (fFull && dwLength < dwMaxLength)         // 读之前是 full,现在读后有一定空间,通知可写
		{
			SetEvent(m_hWriteableEvent);	// now writeable (only set if was full)
		}
	}

	// release mutex
	ReleaseMutex(m_hMutex);

	return iBytesRead;
}

/********************************************************************************
函数名   : <Close>
功能     : 关闭管道的写端
参数     : [OUT] 
           [IN]  
返回值   : 
抛出异常 : 
--------------------------------------------------------------------------------
备注     : 
典型用法 : 
--------------------------------------------------------------------------------
作者     : <hrg>  
创建时间 : 2018/03/18 
--------------------------------------------------------------------------------
修改记录 : 
日 期        版本 修改人       修改内容 

*******************************************************************************/
void CFmpWPoint::Close()
{
	if (m_hReadableEvent != NULL)
	{
		CloseHandle(m_hReadableEvent);
		m_hReadableEvent = NULL;
	}

	if (m_hWriteableEvent != NULL)
	{
		// 通知 Write() 里的 WaitForSingleObject(m_hWriteableEvent, INFINITE) 退出
		SetEvent(m_hWriteableEvent);

		CloseHandle(m_hWriteableEvent);
		m_hWriteableEvent = NULL;
	}

	CFileMemory::Close();
}


/********************************************************************************
函数名   : <Close>
功能     : 关闭管道的读端
参数     : [OUT] 
           [IN]  
返回值   : 
抛出异常 : 
--------------------------------------------------------------------------------
备注     : 
典型用法 : 
--------------------------------------------------------------------------------
作者     : <hrg>  
创建时间 : 2018/03/18 
--------------------------------------------------------------------------------
修改记录 : 
日 期        版本 修改人       修改内容 

*******************************************************************************/
void CFmpRPoint::Close()
{
	if (m_hReadableEvent != NULL)
	{
		// 通知 Read() 里的 WaitForSingleObject(m_hReadableEvent, INFINITE) 退出
		SetEvent(m_hReadableEvent);

		CloseHandle(m_hReadableEvent);
		m_hReadableEvent = NULL;
	}

	if (m_hWriteableEvent != NULL)
	{
		CloseHandle(m_hWriteableEvent);
		m_hWriteableEvent = NULL;
	}

	CFileMemory::Close();
}



DWORD CFileMemoryPipe::Write(BYTE * pBuf, DWORD dwBufLen)
{
	DWORD	iBytesWritten = 0;

	if(pBuf == NULL)
	{
		return iBytesWritten;
	}

	while (dwBufLen > 0)
	{
		// 等待事件 成为可写
		WaitForSingleObject(m_hWriteableEvent, INFINITE);

		// 通过互斥量访问 (5秒后超时)
		DWORD dwWaitResult = WaitForSingleObject(m_hMutex, 5000);
		if (dwWaitResult == WAIT_FAILED || dwWaitResult == WAIT_TIMEOUT)
		{
			ReleaseMutex(m_hMutex);
			return 0;
		}

		DWORD dwOriginalLength = ((DWORD*)m_pBuffer)[0];
		DWORD dwCurrentLength = dwOriginalLength;
		DWORD dwMaxLength = m_dwBufferLength - sizeof(DWORD);
		//if (dwCurrentLength < dwMaxLength)									// 还有位置可写
		if ((dwCurrentLength < dwMaxLength)
			&& (dwMaxLength - dwCurrentLength >= dwBufLen))						// 还有位置可写，而且 space >= dwBufLen，保证要一次写完
		{		
			DWORD dwLengthToWriteNow = dwBufLen;								// 要写入的数据长度
			if (dwCurrentLength + dwBufLen > dwMaxLength)						// 如果要写的长度超出范围的话
			{
				dwLengthToWriteNow = dwMaxLength - dwCurrentLength;				// 要写入的长度 为剩下的长度
			}

			// copy data into shared memory
			memcpy(((unsigned char*)&((DWORD*)m_pBuffer)[1]) + dwCurrentLength, pBuf, dwLengthToWriteNow);			//跳过两个字的位置 并且加上当前写有的长度 
			// write length into shared memory
			dwCurrentLength += dwLengthToWriteNow;								// 现在有的数据长度
			((DWORD*)m_pBuffer)[0] = dwCurrentLength;							// 保存现在数据长度
			if (dwCurrentLength >= (DWORD)dwMaxLength)							// 如果满了，就释放信号，不再写数据了
			{
				ResetEvent(m_hWriteableEvent);			// buffer full, no longer writeable
			}
			// keep track of total bytes written during this writePipe() call
			iBytesWritten += dwLengthToWriteNow;								// 已经写入的多少个数据					
			// adjust data pointer and length for next write
			pBuf += iBytesWritten;												// 指针移动
			dwBufLen -= iBytesWritten;											// 减去已经写入数据的长度
		}
		else if (dwMaxLength - dwCurrentLength < dwBufLen)
		{
			//assert(0);
			//RETAILMSG(1,(L"CFileMemoryPipe::Write:WARM!!!"));
		}

		// signal data available for reading
		if (dwOriginalLength == 0)     // 写之前为空,而现在就可读了				//
		{	// only signal if there was no data in the buffer
			SetEvent(m_hReadableEvent);
		}

		// release mutex
		ReleaseMutex(m_hMutex);
	}

	return iBytesWritten;
}


CXSeries::CXSeries(void)
{
	m_pProtOwer = NULL;
	m_hCom = NULL;
	m_pbyReadBuf = NULL;
	memset(&m_olWrite, 0, sizeof(m_olWrite));
	memset(&m_olWaite, 0, sizeof(m_olWaite));
	memset(&m_olRead, 0, sizeof(m_olRead));
}

CXSeries::~CXSeries(void)
{
}

/*
*函数介绍：打开串口
*入口参数：pPortOwner	:使用此串口类的窗体句柄
		   portNo		:串口号
		   baud			:波特率
		   parity		:奇偶校验
		   databits		:数据位
		   stopbits		:停止位
*出口参数：TREU 成功， FALSE 失败
*返回值：TRUE:成功打开串口;FALSE:打开串口失败
*/
DWORD CXSeries::OPenSeries(IPortOwer* pPortOwer, //串口类的拥有者
						  UINT port,            //串口号
						  UINT baud ,           //波特率
						  UINT parity,          //奇偶校验位
						  UINT databits,        //数据位
						  UINT stopbits         //停止位
						  )
{
	DCB dcb;
	TCHAR szProtname[15];
	DWORD returnval = (DWORD)INVALID_HANDLE_VALUE;
	if(m_hCom == INVALID_HANDLE_VALUE)
	{
		returnval = true;
		goto errorRet;
	}else if(m_hCom == NULL)
	{
		
	}else
	{
		
		goto errorRet;
	}

	assert(pPortOwer != NULL);
	assert(port <= 9 && port>=0);
	//格式化 串口名
	_stprintf(szProtname,L"COM%d",port);
	
	//打开串口
	m_hCom = CreateFile(szProtname,                            //串口名
						GENERIC_READ | GENERIC_WRITE,	   //允许读写
						0,                                 //独占方式
						NULL,
						OPEN_EXISTING,                     //打开而不是创建
						FILE_FLAG_OVERLAPPED,
						NULL
						);


	if(m_hCom == INVALID_HANDLE_VALUE)
	{
		goto errorRet;
	}
	//得到当前打开串口的属性参数
	if(!GetCommState(m_hCom,&dcb))
	{
		goto errorRet;
	}

	//重新设置属性，设置超时特性为立即返回
	dcb.DCBlength = sizeof(DCB);     
	dcb.BaudRate = baud;			        //设置波特率		
	dcb.fBinary = TRUE;				        //设置二进制模式 必须设置true win32 只支持二进制模式
	dcb.fParity = TRUE;				        //设置是否支持奇偶校验位
	dcb.ByteSize = databits;                //数据位 范围：4~8
	dcb.Parity = NOPARITY;			        //校验模式
	dcb.StopBits = stopbits;                //停止位

	dcb.fOutxCtsFlow = false;               //是否监控CTS(clear-to-send)信号来做输出流控
	dcb.fOutxDsrFlow = false;               //是否监控DSR (data-set-ready) 信号来做输出流控
	dcb.fDtrControl = DTR_CONTROL_ENABLE;
	//DTR flow control type
	dcb.fDsrSensitivity = FALSE;			// 通讯设备是否对DSR信号敏感。若设置为TRUE，则当DSR为低时将会忽略所有接收的字节 
	dcb.fTXContinueOnXoff = TRUE;			// 当输入缓冲区满且驱动程序已发出XOFF字符时，是否停止发送。 
	dcb.fOutX = FALSE;					    // XON/XOFF 流量控制在发送时是否可用。如果为TRUE, 当 XOFF 值被收到的时候，发送停止；当 XON 值被收到的时候，发送继续 
	dcb.fInX = FALSE;						// XON/XOFF 流量控制在接收时是否可用。如果为TRUE, 当 输入缓冲区已接收满XoffLim 字节时，发送XOFF字符
	dcb.fErrorChar = FALSE;				    // 指定ErrorChar字符（代替接收到的奇偶校验发生错误时的字节）
	dcb.fNull = FALSE;					    // 接收时是否自动去掉0值
	dcb.fRtsControl = RTS_CONTROL_ENABLE; 
	// RTS flow control 
	dcb.fAbortOnError = FALSE;			    // 当串口发生错误，并不终止串口读写

	if(!SetCommState(m_hCom,&dcb))
	{
		DWORD dw =GetLastError();
		int i = 9;
		goto errorRet;
	}

	// 设置串口读写时间
	COMMTIMEOUTS CommTimeOuts;
	GetCommTimeouts(m_hCom,&CommTimeOuts);
	CommTimeOuts.ReadIntervalTimeout = 5;           // 读间隔超时
	CommTimeOuts.ReadTotalTimeoutConstant = 0;	    // 读时间常量
	CommTimeOuts.ReadTotalTimeoutMultiplier = 0;    // 读时间系数
	CommTimeOuts.WriteTotalTimeoutConstant = 10;	// 写时间常量
	CommTimeOuts.WriteTotalTimeoutMultiplier = 20; // 写时间系数

	if(!SetCommTimeouts(m_hCom,&CommTimeOuts))
	{
		goto errorRet;
	}

	m_pProtOwer = pPortOwer;

	//分配读线程buf
	if(!m_pbyReadBuf)
	{
		//m_pbyReadBuf = new BYTE[READ_THREAD_BUFSIZE];
		m_pbyReadBuf = new BYTE[READ_THREAD_BUFSIZE];
		if (!m_pbyReadBuf)
		{
			goto errorRet;
		}
	}

	// 初始化线程结束标志 
	m_fRThreadExit = FALSE;
	m_fWThreadExit = FALSE;
	m_olWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_olWaite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_olRead.hEvent  = CreateEvent(NULL, TRUE, FALSE, NULL);

	// 创建读串口线程.
	m_hRthread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)RThread,this,0,&m_hRthreadID);
	// 构造消息队列名称,因为不同EXE实列必须有自己的消息队列名称
	TCHAR szQuquename[64]=L"";
	_tcscpy(szQuquename,WRITETHERAD_COM_PORT);
	_tcscat(szQuquename,szProtname);
	
	_tcscat(szQuquename,L"W");
    // 创建共享内存块
	BOOL bfristCreate;
	if (FALSE == m_objWCpu2McuPipe.Create(szQuquename, READ_THREAD_BUFSIZE, OPEN_EXISTING,bfristCreate))
	{
		//MessageBox(NULL, L"获取共享内存失败(在CommonSeries中)", NULL, MB_OK);
		MessageBox(NULL,L"CXSeries::OpenPort: m_objWCpu2McuPipe.Create fail!\r\n",NULL, MB_OK);
	}
	_tcscat(szQuquename,L"R");
	if(FALSE ==  m_objRCpu2McuPipe.Create(szQuquename, READ_THREAD_BUFSIZE, OPEN_EXISTING,bfristCreate))
	{
		MessageBox(NULL,L"CXSeries::OpenPort: m_objRCpu2McuPipe.Create fail!\r\n",NULL, MB_OK);
	}
	// 创建写串口线程.
	m_hWthread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WThread, this, 0, &m_hWthreadID);
	returnval = true;
errorRet:
	if((DWORD)INVALID_HANDLE_VALUE == returnval)
	{
		//ClosePort();
		MessageBox(NULL,L"The serial port has been opened !\r\n",NULL, MB_OK);
		returnval = false;
	}
	return returnval;

}

DWORD CXSeries::WThread(LPVOID pParam)
{
	CXSeries *pseries =  (CXSeries*)(pParam);
	return pseries->WThreadProc();
}

DWORD CXSeries::RThread(LPVOID pParam)
{
	CXSeries *pseries =  (CXSeries*)(pParam);
	return pseries->RThreadProc( pParam);
}

DWORD CXSeries::WThreadProc()
{
	BYTE bRBuf[2048] = {0};
	while (!this->m_fWThreadExit)
	{
		DWORD dwlen = m_objWCpu2McuPipe.Read(bRBuf,2048);
		if(dwlen>0)
	    this->WritePort(m_hCom,bRBuf,dwlen);
		Sleep(10);
	}
	return this->m_fWThreadExit;
}

DWORD CXSeries::RThreadProc(LPVOID pParam)
{
	CXSeries *pseries =  (CXSeries*)(pParam);
	DWORD dwEvtMask;
	DWORD dwActualReadLen = 0;
	DWORD dwWillReadLen = READ_THREAD_BUFSIZE -1;
	// 清空缓冲, 并检查串口是否打开.
	assert(m_hCom !=INVALID_HANDLE_VALUE);
	// 指定端口监测的事件集.
	SetCommMask(m_hCom, EV_RXCHAR);

	// 分配设备缓冲区， 使用默认的。初始化缓冲区中的信息.
	PurgeComm(m_hCom, PURGE_TXCLEAR | PURGE_RXCLEAR);
	while(!m_fRThreadExit)
	{
		WaitCommEvent(m_hCom, &dwEvtMask, &m_olWaite);
		if(FALSE == GetOverlappedResult(m_hCom,&m_olWaite,&dwActualReadLen,TRUE))
		{
			//GetOverlappedResult
			
			if (ERROR_IO_PENDING != GetLastError())  //等待重叠操作
			{
				MessageBox(NULL, L"11111111111ERROR_IO_PENDING != GetLastError()", NULL, MB_OK);
				continue;
			}
			//清除 error flag
			DWORD dwErrors;
			COMSTAT comStat;
			memset(&comStat, 0, sizeof(comStat));
			//清除硬件的通讯错误 ,获取设备的当前状态
			ClearCommError(m_hCom, &dwErrors, &comStat);

			MessageBox(NULL, L"FALSE == GetOverlappedResult(...)", NULL, MB_OK);
			continue;
		}

		if(FALSE ==  ReadFile(m_hCom, m_pbyReadBuf, dwWillReadLen, &dwActualReadLen, &m_olRead))
		{
			DWORD fff = GetLastError();
			if (ERROR_IO_PENDING != GetLastError())
			{
				MessageBox(NULL, L"222222222222ERROR_IO_PENDING != GetLastError()", NULL, MB_OK);
				continue;
			}
			if (FALSE == GetOverlappedResult(m_hCom, &m_olRead, &dwActualReadLen, TRUE))
			{
				MessageBox(NULL, L"FALSE == GetOverlappedResult(...)", NULL, MB_OK);
				continue;
			}else
			{
				if(dwActualReadLen >0 )
				{
					// 触发读取回调函数.
					if (m_pProtOwer)
					{
						m_pProtOwer->OnSeriesRead(m_pbyReadBuf, dwActualReadLen, NULL);
					}
				}
			}
		}else
		{

			if(dwActualReadLen >0 )
			{
				// 触发读取回调函数.
				if (m_pProtOwer)
				{
					m_pProtOwer->OnSeriesRead(m_pbyReadBuf, dwActualReadLen, NULL);
				}
			}
		}

	}
	return this->m_fRThreadExit;
}

/*
*函数介绍：向串口发送数据
*入口参数：pbyBuf		: 将要往串口写入的数据的缓冲区
		   dwBufLen	: 将要往串口写入的数据的缓冲区长度
*出口参数：(无)
*返回值：TRUE:表示成功地将要发送的数据传递到写线程消息队列。
		 FALSE:表示将要发送的数据传递到写线程消息队列失败。
		 注视：此处的TRUE,不直接代表数据一定成功写入到串口了。
*/
BOOL CXSeries::OwerWritePort(const BYTE * pbyBuf, DWORD dwBufLen)
{
	DWORD dwActual;
	BOOL bRet = TRUE;
	if(m_hCom != INVALID_HANDLE_VALUE)
	{
		dwActual = m_objWCpu2McuPipe.Write((BYTE *)pbyBuf, dwBufLen);
		assert(dwActual == dwBufLen);
	}else
	{
		BOOL bRet = FALSE;
	}
	
	return bRet;
    
}

// 私用方法, 用于向串口写数据, 被写线程调用.
BOOL CXSeries::WritePort(HANDLE hComm, const BYTE *pbyBuf, DWORD dwBufLen)
{
	DWORD dwNumBytesWritten;
	DWORD dwHaveNumWritten = 0;          // 已经写入多少.
	BYTE pby = pbyBuf[9];
	assert(hComm != INVALID_HANDLE_VALUE);
	do
	{
		DWORD dwActualWrite;  
		if (FALSE == WriteFile(hComm,    // 串口句柄.
			pbyBuf + dwHaveNumWritten,   // 被写数据缓冲区.
			dwBufLen - dwHaveNumWritten, // 被写数据缓冲区大小.
			&dwNumBytesWritten,          // 函数执行成功后, 返回实际向串口写的个数.
			&m_olWrite))
		{
			if (ERROR_IO_PENDING != GetLastError())
			{
				MessageBox(NULL, L"33333333333333333ERROR_IO_PENDING != GetLastError()", NULL, MB_OK);
				return FALSE;
			}
		}
		if (TRUE == GetOverlappedResult(hComm, &m_olWrite, &dwNumBytesWritten, TRUE))
		{
			dwHaveNumWritten = dwHaveNumWritten + dwNumBytesWritten;
			// 写入完成.
			if(dwHaveNumWritten == dwBufLen)
			{
				break;
			}
		}
		else
		{
			MessageBox(NULL, L"写串口失败", NULL, MB_OK);
			return FALSE;
		}
	} while(TRUE);

	return TRUE;
}

// 关闭读线程.
void CXSeries::CloseRThread()
{
	if (m_hCom != INVALID_HANDLE_VALUE) 
	{
		// 设置所有事件无效.
		SetCommMask(m_hCom, 0);

		// 清空所有将要读的数据.
		PurgeComm(m_hCom, PURGE_RXCLEAR);
	}

	if(m_hRthread) 
	{
		m_fRThreadExit = TRUE;

		// 等待3秒, 6次,每次500ms 如果读线程没有退出, 则强制退出.
		int i = 0;
		while (i < 6)
		{
			//SetEvent(m_hComm);      // 这个不能
			SetEvent(m_hRthread);  // 这个能让读线程收到事件

			if (WaitForSingleObject(m_hRthread, 500) == WAIT_TIMEOUT)
			{
				i++;
			}
			else
			{
				break;
			}
		}
		if (i == 10)
		{

			TerminateThread(m_hRthread, 0);        
		}

		CloseHandle(m_hRthread);
		m_hRthread = NULL;
	}
}

// 关闭写线程.
void CXSeries::CloseWThread()
{
	if(m_hCom != INVALID_HANDLE_VALUE) 
	{
		// 清空所有将要写的数据.
		PurgeComm(m_hCom, PURGE_TXCLEAR);
	}

	// 等待3秒，如果读线程没有退出，则强制退出.
	if(m_hWthread) 
	{
		m_fWThreadExit = TRUE;

		SetEvent(m_hWthread);

		// 关闭读端和写端
		m_objRCpu2McuPipe.Close();
		m_objWCpu2McuPipe.Close();

		if(WaitForSingleObject(m_hWthread, 3000) == WAIT_TIMEOUT)
		{

			TerminateThread(m_hWthread, 0);
		}

		CloseHandle(m_hWthread);
		m_hWthread = NULL;
	}
}

void CXSeries::ClosePort()
{
	// 关闭读线程.
	CloseRThread();

	// 关闭写线程.
	CloseWThread();

	// 关闭串口.
	if(m_hCom != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hCom);
		m_hCom = INVALID_HANDLE_VALUE;
	}

	if (m_pbyReadBuf)
	{
		delete []m_pbyReadBuf;
		m_pbyReadBuf = NULL;
	}

	if (m_olWrite.hEvent)
	{
		CloseHandle(m_olWrite.hEvent);
		m_olWrite.hEvent = NULL;
	}

	if (m_olWaite.hEvent)
	{
		CloseHandle(m_olWaite.hEvent);
		m_olWaite.hEvent = NULL;
	}

	if (m_olRead.hEvent)
	{
		CloseHandle(m_olRead.hEvent);
		m_olRead.hEvent = NULL;
	}


	return;
}
