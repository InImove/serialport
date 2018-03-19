#include "StdAfx.h"
#include "XSeries.h"
 #include <assert.h>
#include <WinDef.h>
/**************************************************************************************************************************************
*                                                                                                                                     *
* Class CFileMemory                                                                                                                   *  
* ���̼�̶���С���ݿ鹲��                                                                                                            *  
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
	m_hMutex = CreateMutex(NULL, FALSE, szMutexName);   // ����������
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
	m_hFileMapping = CreateFileMapping((HANDLE)INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, m_dwBufferLength, szMemName); // ���������ڴ�
	if (m_hFileMapping == NULL)
	{
		goto cleanup;
	}

	m_pBuffer = (BYTE*)MapViewOfFile(m_hFileMapping, FILE_MAP_WRITE, 0, 0, 0);  // ��ȡӳ����ͼ�ļ��Ŀ�ʼ�ĵ�ַ
	if (NULL == m_pBuffer)
		goto cleanup;

	if (bFirstCreate)
	{
		memset(m_pBuffer, 0, m_dwBufferLength); // ע��,����ǹ���Ļ��Ͳ��������
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
* ���̼����ݿ鹲��                                                                                                                    *  
* ����ͬʱһ��һд                                                                                                                    *  
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
		// ֪ͨ Read() ��� WaitForSingleObject(m_hReadableEvent, INFINITE) �˳�
		SetEvent(m_hReadableEvent);
		CloseHandle(m_hReadableEvent);
		m_hReadableEvent = NULL;
	}

	if (m_hWriteableEvent != NULL)
	{
		// ֪ͨ Write() ��� WaitForSingleObject(m_hReadableEvent, INFINITE) �˳�
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
	WaitForSingleObject(m_hReadableEvent, INFINITE);
	// serialise access through mutex (timeout after 5 seconds)
	DWORD dwWaitResult = WaitForSingleObject(m_hMutex, 5000);
	if (dwWaitResult == WAIT_FAILED || dwWaitResult == WAIT_TIMEOUT)
	{
		return 0;
	}

	// get length from shared memory
	DWORD  dwLength = ((DWORD*)m_pBuffer)[0];									// ��ȡ�ô������ݵĳ���
	DWORD  dwMaxLength = m_dwBufferLength - sizeof(DWORD);						// ������ݴ���
	bool   fFull = (dwLength >= dwMaxLength);									// �����Ƿ�����

	//if (0 == dwLength)
	//{
	// �յ���֪ͨ����Ϊ���� Read �˳��ȴ�
	//	DebugBreak();
	//}

	// copy data
	if (dwBufLen >= dwLength)													// ���Ҫ��ȡ�����ݴ��������е�����
	{
		iBytesRead = dwLength;													// �ɶ����ݳ���			
		memcpy(pBuf, m_pBuffer + sizeof(DWORD), iBytesRead);					// ��ȡ����
		((DWORD*)m_pBuffer)[0] = 0;												// �����ݳ�����Ϊ0 
		ResetEvent(m_hReadableEvent);		// no data left for reading			// �ͷŶ��ź� 
		if (fFull)                           // ��֮ǰ�� full,���ڶ���ȫΪ��,֪ͨ��д
		{
			SetEvent(m_hWriteableEvent);	// now writeable (only set if was full)
		}
	}
	else																		// �����ȡ��������С�������е�������	
	{
		iBytesRead = dwBufLen;
		memcpy(pBuf, m_pBuffer + sizeof(DWORD), iBytesRead);					
		dwLength -= iBytesRead;													// ��ȡ����			
		((DWORD*)m_pBuffer)[0] = dwLength;										// ���¼��㳤��
		memmove(m_pBuffer + sizeof(DWORD), m_pBuffer + sizeof(DWORD) + iBytesRead, dwLength);
		if (fFull && dwLength < dwMaxLength)         // ��֮ǰ�� full,���ڶ�����һ���ռ�,֪ͨ��д
		{
			SetEvent(m_hWriteableEvent);	// now writeable (only set if was full)
		}
	}

	// release mutex
	ReleaseMutex(m_hMutex);

	return iBytesRead;
}

/********************************************************************************
������   : <Close>
����     : �رչܵ���д��
����     : [OUT] 
           [IN]  
����ֵ   : 
�׳��쳣 : 
--------------------------------------------------------------------------------
��ע     : 
�����÷� : 
--------------------------------------------------------------------------------
����     : <hrg>  
����ʱ�� : 2018/03/18 
--------------------------------------------------------------------------------
�޸ļ�¼ : 
�� ��        �汾 �޸���       �޸����� 

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
		// ֪ͨ Write() ��� WaitForSingleObject(m_hWriteableEvent, INFINITE) �˳�
		SetEvent(m_hWriteableEvent);

		CloseHandle(m_hWriteableEvent);
		m_hWriteableEvent = NULL;
	}

	CFileMemory::Close();
}


/********************************************************************************
������   : <Close>
����     : �رչܵ��Ķ���
����     : [OUT] 
           [IN]  
����ֵ   : 
�׳��쳣 : 
--------------------------------------------------------------------------------
��ע     : 
�����÷� : 
--------------------------------------------------------------------------------
����     : <hrg>  
����ʱ�� : 2018/03/18 
--------------------------------------------------------------------------------
�޸ļ�¼ : 
�� ��        �汾 �޸���       �޸����� 

*******************************************************************************/
void CFmpRPoint::Close()
{
	if (m_hReadableEvent != NULL)
	{
		// ֪ͨ Read() ��� WaitForSingleObject(m_hReadableEvent, INFINITE) �˳�
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

	if ((m_pBuffer == NULL) || (pBuf == NULL))
	{
		return iBytesWritten;
	}

	while (dwBufLen > 0)
	{
		// �ȴ��¼� ��Ϊ��д
		WaitForSingleObject(m_hWriteableEvent, INFINITE);

		// ͨ������������ (5���ʱ)
		DWORD dwWaitResult = WaitForSingleObject(m_hMutex, 5000);
		if (dwWaitResult == WAIT_FAILED || dwWaitResult == WAIT_TIMEOUT)
		{
			return 0;
		}

		DWORD dwOriginalLength = ((DWORD*)m_pBuffer)[0];
		DWORD dwCurrentLength = dwOriginalLength;
		DWORD dwMaxLength = m_dwBufferLength - sizeof(DWORD);
		//if (dwCurrentLength < dwMaxLength)									// ����λ�ÿ�д
		if ((dwCurrentLength < dwMaxLength)
			&& (dwMaxLength - dwCurrentLength >= dwBufLen))						// ����λ�ÿ�д������ space >= dwBufLen����֤Ҫһ��д��
		{		
			DWORD dwLengthToWriteNow = dwBufLen;								// Ҫд������ݳ���
			if (dwCurrentLength + dwBufLen > dwMaxLength)						// ���Ҫд�ĳ��ȳ�����Χ�Ļ�
			{
				dwLengthToWriteNow = dwMaxLength - dwCurrentLength;				// Ҫд��ĳ��� Ϊʣ�µĳ���
			}

			// copy data into shared memory
			memcpy(((unsigned char*)&((DWORD*)m_pBuffer)[1]) + dwCurrentLength, pBuf, dwLengthToWriteNow);			//���������ֵ�λ�� ���Ҽ��ϵ�ǰд�еĳ��� 
			// write length into shared memory
			dwCurrentLength += dwLengthToWriteNow;								// �����е����ݳ���
			((DWORD*)m_pBuffer)[0] = dwCurrentLength;							// �����������ݳ���
			if (dwCurrentLength >= (DWORD)dwMaxLength)							// ������ˣ����ͷ��źţ�����д������
			{
				ResetEvent(m_hWriteableEvent);			// buffer full, no longer writeable
			}
			// keep track of total bytes written during this writePipe() call
			iBytesWritten += dwLengthToWriteNow;								// �Ѿ�д��Ķ��ٸ�����					
			// adjust data pointer and length for next write
			pBuf += iBytesWritten;												// ָ���ƶ�
			dwBufLen -= iBytesWritten;											// ��ȥ�Ѿ�д�����ݵĳ���
		}
		else if (dwMaxLength - dwCurrentLength < dwBufLen)
		{
			//assert(0);
			//RETAILMSG(1,(L"CFileMemoryPipe::Write:WARM!!!"));
		}

		// signal data available for reading
		if (dwOriginalLength == 0)     // д֮ǰΪ��,�����ھͿɶ���				//
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
}

CXSeries::~CXSeries(void)
{
}

/*
*�������ܣ��򿪴���
*��ڲ�����pPortOwner	:ʹ�ô˴�����Ĵ�����
		   portNo		:���ں�
		   baud			:������
		   parity		:��żУ��
		   databits		:����λ
		   stopbits		:ֹͣλ
*���ڲ�����TREU �ɹ��� FALSE ʧ��
*����ֵ��TRUE:�ɹ��򿪴���;FALSE:�򿪴���ʧ��
*/
DWORD CXSeries::OPenSeries(IPortOwer* pPortOwer, //�������ӵ����
						  UINT port,            //���ں�
						  UINT baud ,           //������
						  UINT parity,          //��żУ��λ
						  UINT databits,        //����λ
						  UINT stopbits         //ֹͣλ
						  )
{
	DCB dcb;
	TCHAR szProtname[15];
	DWORD returnval = (DWORD)INVALID_HANDLE_VALUE;
	if(m_hCom != INVALID_HANDLE_VALUE)
	{
		returnval = true;
		goto errorRet;
	}

	assert(pPortOwer != NULL);
	assert(port <= 9 && port>=0);
	//��ʽ�� ������
	_stprintf(szProtname,L"COM%d",port);
	
	//�򿪴���
	m_hCom = CreateFile(szProtname,                            //������
						GENERIC_READ | GENERIC_WRITE,	   //�����д
						0,                                 //��ռ��ʽ
						NULL,
						OPEN_EXISTING,                     //�򿪶����Ǵ���
						0,
						NULL
						);
	if(m_hCom == INVALID_HANDLE_VALUE)
	{
		goto errorRet;
	}
	//�õ���ǰ�򿪴��ڵ����Բ���
	if(!GetCommState(m_hCom,&dcb))
	{
		goto errorRet;
	}

	//�����������ԣ����ó�ʱ����Ϊ��������
	dcb.DCBlength = sizeof(DCB);     
	dcb.BaudRate = baud;			        //���ò�����		
	dcb.fBinary = true;				        //���ö�����ģʽ ��������true win32 ֻ֧�ֶ�����ģʽ
	dcb.fParity = true;				        //�����Ƿ�֧����żУ��λ
	dcb.ByteSize = databits;                //����λ ��Χ��4~8
	dcb.Parity = NOPARITY;			        //У��ģʽ
	dcb.StopBits = stopbits;                //ֹͣλ

	dcb.fOutxCtsFlow = false;               //�Ƿ���CTS(clear-to-send)�ź������������
	dcb.fOutxDsrFlow = false;               //�Ƿ���DSR (data-set-ready) �ź������������
	dcb.fDtrControl = DTR_CONTROL_ENABLE;
	//DTR flow control type
	dcb.fDsrSensitivity = FALSE;			// ͨѶ�豸�Ƿ��DSR�ź����С�������ΪTRUE����DSRΪ��ʱ����������н��յ��ֽ� 
	dcb.fTXContinueOnXoff = TRUE;			// �����뻺�����������������ѷ���XOFF�ַ�ʱ���Ƿ�ֹͣ���͡� 
	dcb.fOutX = FALSE;					    // XON/XOFF ���������ڷ���ʱ�Ƿ���á����ΪTRUE, �� XOFF ֵ���յ���ʱ�򣬷���ֹͣ���� XON ֵ���յ���ʱ�򣬷��ͼ��� 
	dcb.fInX = FALSE;						// XON/XOFF ���������ڽ���ʱ�Ƿ���á����ΪTRUE, �� ���뻺�����ѽ�����XoffLim �ֽ�ʱ������XOFF�ַ�
	dcb.fErrorChar = FALSE;				    // ָ��ErrorChar�ַ���������յ�����żУ�鷢������ʱ���ֽڣ�
	dcb.fNull = FALSE;					    // ����ʱ�Ƿ��Զ�ȥ��0ֵ
	dcb.fRtsControl = RTS_CONTROL_ENABLE; 
	// RTS flow control 
	dcb.fAbortOnError = FALSE;			    // �����ڷ������󣬲�����ֹ���ڶ�д

	if(!SetCommState(m_hCom,&dcb))
	{
		goto errorRet;
	}

	// ���ô��ڶ�дʱ��
	COMMTIMEOUTS CommTimeOuts;
	GetCommTimeouts(m_hCom,&CommTimeOuts);
	CommTimeOuts.ReadIntervalTimeout = 5;           // �������ʱ
	CommTimeOuts.ReadTotalTimeoutConstant = 0;	    // ��ʱ�䳣��
	CommTimeOuts.ReadTotalTimeoutMultiplier = 0;    // ��ʱ��ϵ��
	CommTimeOuts.WriteTotalTimeoutConstant = 10;	// дʱ�䳣��
	CommTimeOuts.WriteTotalTimeoutMultiplier = 100; // дʱ��ϵ��

	if(!SetCommTimeouts(m_hCom,&CommTimeOuts))
	{
		goto errorRet;
	}

	m_pProtOwer = pPortOwer;

	//������߳�buf
	if(!m_pbyReadBuf)
	{
		//m_pbyReadBuf = new BYTE[READ_THREAD_BUFSIZE];
		m_pbyReadBuf = new BYTE[READ_THREAD_BUFSIZE];
		if (!m_pbyReadBuf)
		{
			goto errorRet;
		}
	}

	// ��ʼ���߳̽�����־ 
	m_fRThreadExit = FALSE;
	m_fWThreadExit = FALSE;
	// �����������߳�.
	m_hRthread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)RThread,this,0,&m_hRthreadID);
	// ������Ϣ��������,��Ϊ��ͬEXEʵ�б������Լ�����Ϣ��������
	TCHAR szQuquename[64]=L"";
	_tcscpy(szQuquename,WRITETHERAD_COM_PORT);
	_tcscat(szQuquename,szProtname);
	
	_tcscat(szQuquename,L"W");
    // ���������ڴ��
	BOOL bfristCreate;
	if (FALSE == m_objWCpu2McuPipe.Create(szQuquename, READ_THREAD_BUFSIZE, OPEN_EXISTING,bfristCreate))
	{
		//MessageBox(NULL, L"��ȡ�����ڴ�ʧ��(��CommonSeries��)", NULL, MB_OK);
		MessageBox(NULL,L"CXSeries::OpenPort: m_objWCpu2McuPipe.Create fail!\r\n",NULL, MB_OK);
	}
	_tcscat(szQuquename,L"R");
	if(FALSE ==  m_objRCpu2McuPipe.Create(szQuquename, READ_THREAD_BUFSIZE, OPEN_EXISTING,bfristCreate))
	{
		MessageBox(NULL,L"CXSeries::OpenPort: m_objRCpu2McuPipe.Create fail!\r\n",NULL, MB_OK);
	}
	// ����д�����߳�.
	m_hWthread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WThread, this, 0, &m_hWthreadID);
	returnval = true;
errorRet:
	if((DWORD)INVALID_HANDLE_VALUE == returnval)
	{
		ClosePort();
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
	return pseries->RThreadProc();
}

DWORD CXSeries::WThreadProc()
{
	BYTE bRBuf[2048] = {0};
	while (!this->m_fWThreadExit)
	{
		DWORD dwlen = m_objRCpu2McuPipe.Read(bRBuf,2048);
		return this->WritePort(m_hCom,bRBuf,dwlen);
	}
	return 0;
}

DWORD CXSeries::RThreadProc()
{
	DWORD dwEvtMask;
	DWORD dwActualReadLen = 0;
	DWORD dwWillReadLen = 0;
	// ��ջ���, ����鴮���Ƿ��.
	assert(m_hCom !=INVALID_HANDLE_VALUE);
	// ָ���˿ڼ����¼���.
	SetCommMask(m_hCom, EV_RXCHAR);

	// �����豸�������� ʹ��Ĭ�ϵġ���ʼ���������е���Ϣ.
	PurgeComm(m_hCom, PURGE_TXCLEAR | PURGE_RXCLEAR);
	while(!m_fRThreadExit)
	{
		WaitCommEvent(m_hCom, &dwEvtMask, &m_olWaite);
		if(FALSE == GetOverlappedResult(m_hCom,&m_olWaite,&dwActualReadLen,TRUE))
		{
			if (ERROR_IO_PENDING != GetLastError())  //�ȴ��ص�����
			{
				MessageBox(NULL, L"ERROR_IO_PENDING != GetLastError()", NULL, MB_OK);
				continue;
			}
			//��� error flag
			DWORD dwErrors;
			COMSTAT comStat;
			memset(&comStat, 0, sizeof(comStat));
			//���Ӳ����ͨѶ���� ,��ȡ�豸�ĵ�ǰ״̬
			ClearCommError(m_hCom, &dwErrors, &comStat);

			MessageBox(NULL, L"FALSE == GetOverlappedResult(...)", NULL, MB_OK);
			continue;
		}

		if(FALSE ==  ReadFile(m_hCom, m_pbyReadBuf, dwWillReadLen, &dwActualReadLen, &m_olRead))
		{
			if (ERROR_IO_PENDING != GetLastError())
			{
				MessageBox(NULL, L"ERROR_IO_PENDING != GetLastError()", NULL, MB_OK);
				continue;
			}
			if (FALSE == GetOverlappedResult(m_hCom, &m_olRead, &dwActualReadLen, TRUE))
			{
				MessageBox(NULL, L"FALSE == GetOverlappedResult(...)", NULL, MB_OK);
				continue;
			}
		}else
		{

			if(dwActualReadLen >0 )
			{
				// ������ȡ�ص�����.
				if (m_pProtOwer)
				{
					m_pProtOwer->OnSeriesRead(m_pbyReadBuf, dwActualReadLen, NULL);
				}
			}
		}

	}
	return this->m_fRThreadExit;
}
// ˽�÷���, �����򴮿�д����, ��д�̵߳���.
BOOL CXSeries::WritePort(HANDLE hComm, const BYTE *pbyBuf, DWORD dwBufLen)
{
	DWORD dwNumBytesWritten;
	DWORD dwHaveNumWritten = 0;          // �Ѿ�д�����.
	assert(hComm != INVALID_HANDLE_VALUE);
	do
	{
		DWORD dwActualWrite;  
		if (FALSE == WriteFile(hComm,    // ���ھ��.
			pbyBuf + dwHaveNumWritten,   // ��д���ݻ�����.
			dwBufLen - dwHaveNumWritten, // ��д���ݻ�������С.
			&dwNumBytesWritten,          // ����ִ�гɹ���, ����ʵ���򴮿�д�ĸ���.
			&m_olWrite))
		{
			if (ERROR_IO_PENDING != GetLastError())
			{
				MessageBox(NULL, L"ERROR_IO_PENDING != GetLastError()", NULL, MB_OK);
				return FALSE;
			}
		}
		if (TRUE == GetOverlappedResult(hComm, &m_olWrite, &dwNumBytesWritten, TRUE))
		{
			dwHaveNumWritten = dwHaveNumWritten + dwNumBytesWritten;
			// д�����.
			if(dwHaveNumWritten == dwBufLen)
			{
				break;
			}
		}
		else
		{
			MessageBox(NULL, L"д����ʧ��", NULL, MB_OK);
			return FALSE;
		}
	} while(TRUE);

	return TRUE;
}

void CXSeries::ClosePort()
{

}