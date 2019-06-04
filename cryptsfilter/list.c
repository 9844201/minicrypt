
//#include "sfilter.h"

/////////////////////////////////////////////////////////////////////////////
//
//                 ���� �Լ���غ�����ʵ��
//
/////////////////////////////////////////////////////////////////////////////

//
//��ȡ�ļ�����Ϣ�����͵�Ӧ�ò�    Ϊ��Ҫ���ӵĺ����Լ����ݽṹ��ȫ�ֱ���
//

#define LOGBUFSIZE ((ULONG)(64*0x400-(3*sizeof(ULONG)+1)))//��֤С��64K


typedef struct _log 
{
	ULONG           Len;
	struct _log   * Next;
	CHAR            Data[LOGBUFSIZE];
} LOG_BUF, *PLOG_BUF;


PLOG_BUF            CurrentLog = NULL;
ULONG               NumLog = 0;
ULONG               MaxLog = (1024*1024) / LOGBUFSIZE;
FAST_MUTEX          LogMutex;

//
//����ǰ�Ļ��������˾ͽ��е���   
//���ڷ���һ���µĻ����� ��������������ǰ��
// IOCTL_FILEMON_GETSTATS��driverentry
//

VOID
SfAllocateLog(
	      VOID
	      )
{
	PLOG_BUF prev = CurrentLog, newLog;
	// ���Ѿ����䵽����� ���ͷ�һ�������
	if( MaxLog == NumLog ) {
		KdPrint((" ***** Dropping records *****"));
		CurrentLog->Len = 0;
		return; 
	}
	KdPrint(("SfAllocateLog: num: %d max: %d\n", NumLog, MaxLog ));
	//������ǵ�ǰʹ�õ�����������ǿյģ���ȥʹ���� ��ʵ�ʲ���Ҫ������
	if( !CurrentLog->Len ) {
		return;
	}
	//�����µ�
	newLog =(PLOG_BUF) ExAllocatePool( NonPagedPool, sizeof(*CurrentLog) );
	if( newLog ) { 
		//����ɹ� ��ӵ�����ͷ
		CurrentLog       = newLog;
		CurrentLog->Len  = 0;
		CurrentLog->Next = prev;
		NumLog++;
	} else {
		//ʧ�ܾ��������ڵĻ�����
		CurrentLog->Len = 0;
	}
}
VOID 
SfRecordLog(PANSI_STRING name)
{
	ULONG i=(ULONG)((name->Length)/sizeof(CHAR));
	ULONG k=(CurrentLog->Len);
	ExAcquireFastMutex( &LogMutex );
	if (k+i>=LOGBUFSIZE)
	{
		SfAllocateLog();
	}
	CurrentLog->Len=k+1;
	RtlCopyMemory(((CurrentLog->Data)+k),(name->Buffer),i);
	CurrentLog->Len=k+i+1;
	CurrentLog->Data[k+i]='\n';
	KdPrint(("the new added buffer is %s\n",CurrentLog->Data+k+1));
	//	Kdprint(("len = %d \n",(int)(CurrentLog->Len)));
	ExReleaseFastMutex( &LogMutex ); 
}
VOID 
SfFreeLog(//�ͷ����ǵ�ǰ�Ѿ���������е��������������   Unload
	  VOID 
	  )
{
	PLOG_BUF  prev;	
	ExAcquireFastMutex( &LogMutex );
	while( CurrentLog ) 
	{
		prev = CurrentLog->Next;
		ExFreePool( CurrentLog );
		CurrentLog = prev;
	}   
	ExReleaseFastMutex( &LogMutex ); 
	KdPrint(("SfFreeLog\n"));
}   
PLOG_BUF 
SfGetOldestLog(//��ȡ�����һ����������� IOCTL_FILEMON_GETSTATS
	       VOID 
	       )
{
	PLOG_BUF  ptr = CurrentLog, prev = NULL;
	//��������  
	while( ptr->Next ) {
		ptr = (prev = ptr)->Next;
	}
	//���ÿ������ж��
	if( prev ) {
		prev->Next = NULL;    
		NumLog--;
	}
	return ptr;
}
VOID 
SfResetLog(//�����еĻ��������  ��GUI�˳�ʱ IRP_MJ_CLOSE�е���
	   VOID
	   )
{
	PLOG_BUF  current, next;
	ExAcquireFastMutex( &LogMutex );
	//����
	current = CurrentLog->Next;
	while( current ) {
		//�ͷ�
		next = current->Next;
		ExFreePool( current );
		current = next;
	}
	NumLog = 1;
	CurrentLog->Len = 0;
	CurrentLog->Next = NULL;
	ExReleaseFastMutex( &LogMutex );    
}