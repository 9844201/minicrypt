#include "Ioctlcmd.h"

//����Ҫһ��������������е��ļ�·��
typedef struct _COMPATH
{
	WCHAR	Name[MAXPATHLEN];
	struct _COMPATH *next;
} COMPATH, *PCOMPATH;


//�����ʱ��û��
typedef struct _HIDEPATH
{
	WCHAR	Name[MAXPATHLEN];
	WCHAR    Hider[HIDERLEN];
	struct _HIDEPATH *next;
} HIDEPATH, *PHIDEPATH;

//һ�������ṹ��  Դ��"Ioctlcmd.h"
////��ֹ������ɾ�����ļ���
typedef struct _ALLFile{
	WCHAR   name[MAXRULES][MAXPATHLEN];		// ��Ŷ�����¼
	ULONG		num;						// ʵ��ӵ�еļ�¼
} ALLFile, *PAllFile;

//һ�� ���ص��ļ� ���õ����ݽṹ
typedef struct _Hider{
	WCHAR fatherpath[MAXPATHLEN];//�ļ��ĸ�Ŀ¼
	WCHAR  filename[HIDERLEN];//�ļ���
	WCHAR  hiddenallpath[MAXPATHLEN];
	ULONG flag;
}Hider ,*PHider;


//�����ļ� ���ݿ�
typedef struct _ALLFileHide{
	Hider hider[MAXRULES];
	ULONG		num;						// ʵ��ӵ�еļ�¼
} ALLFileHide, *PAllFileHide;

typedef struct  _Reparser
{
	LIST_ENTRY ListEntry;
	WCHAR Sourcefile[MAXPATHLEN];
	WCHAR Targetfile[MAXPATHLEN];
}Reparser ,*PReparser;

//�ض���///////////////////////////////////////////////////////////////////////////////////////////////////////
static LIST_ENTRY gReparseList;
FAST_MUTEX ReparseMutex;
static ULONG ReparseNum=0;

//��ʼ��������� Ҫ��Driverentry�е�����
//InitializeListHead(&gReparseList);


//��ֹ����//////////////////////////////////////////////////////////////////////////////////////////////////////

PCOMPATH  gNoAccess;
FAST_MUTEX NoAceMutex;
static ULONG NoAceNum=0;

//��ֹɾ��/////////////////////////////////////////////////////////////////////////////////////////////////////

PCOMPATH  gNoDelete;
FAST_MUTEX NoDelMutex;
static ULONG NoDelNum=0;

//�ļ�����ʱ�õ��Ľ�ֹ����/////////////////////////////////////////////////////////////////////////////////////
PCOMPATH  gHidNoAccess;
FAST_MUTEX NoHidAceMutex;
static ULONG NoHidAceNum=0;


//��ֹ���ʵ�����Ԫ�غ���
VOID
SfAddComPathAce(
			 IN PWSTR Name//�����·����
			 )
{
	PCOMPATH temp=gNoAccess,newComPath;
	newComPath= ExAllocatePoolWithTag(PagedPool, sizeof(COMPATH), 'COMP');
	ExAcquireFastMutex( &NoAceMutex );
	if (newComPath)
	{	
	gNoAccess=newComPath;//���µ�Ԫ��ת��Ϊ��ͷ
	wcscpy(gNoAccess->Name,Name);
	gNoAccess->next=temp;//ԭ����head�������
    NoAceNum++;
	}

	ExReleaseFastMutex(&NoAceMutex);
}
//��ֹ���ʵ�ɾ��Ԫ�غ���
VOID
SfDeleteComPathAce(
				   IN PWSTR DelName
				   )
{
	PCOMPATH temp;
    PCOMPATH tempbefore; 
	if (!gNoAccess)
    {
		return;
    }	
	ExAcquireFastMutex( &NoAceMutex );	
	if (!wcscmp(DelName,gNoAccess->Name))
	{
        temp=gNoAccess;
		gNoAccess=gNoAccess->next;            
		ExFreePoolWithTag(temp,'COMP');	
		NoAceNum--;
	}
    else
	{
		for (temp=gNoAccess;temp!=NULL;tempbefore=temp,temp=temp->next)
		{
			if (!wcscmp(DelName,temp->Name))
			{
				tempbefore->next=temp->next;
				ExFreePoolWithTag(temp,'COMP');	
				NoAceNum--;
			}
		}
	}
    ExReleaseFastMutex(&NoAceMutex); 
}

//��ֹɾ������Ԫ�غ���
VOID
SfAddComPathDel(
				IN PWSTR Name//�����·����
				)
{
	PCOMPATH temp=gNoDelete,newComPath;
	newComPath= ExAllocatePoolWithTag(PagedPool, sizeof(COMPATH), 'COMP');
	ExAcquireFastMutex( &NoDelMutex );
	if (newComPath)
	{	
		gNoDelete=newComPath;//���µ�Ԫ��ת��Ϊ��ͷ
		wcscpy(gNoDelete->Name,Name);
		gNoDelete->next=temp;//ԭ����head�������
		NoDelNum++;
//		KdPrint(("#$$$$$$$$$$$$$$$$$  %s\n",gNoDelete->Name));
	}
	
	ExReleaseFastMutex(&NoDelMutex);
}

//��ֹɾ��ɾ��Ԫ�غ���
VOID
SfDeleteComPathDel(
				   IN PWSTR DelName
				   )
{
	PCOMPATH temp;
	PCOMPATH tempbefore; 
	if (!gNoDelete)
	{
		return;
	}	
	ExAcquireFastMutex( &NoDelMutex );	
	if (!wcscmp(DelName,gNoDelete->Name))
	{
		temp=gNoDelete;
		gNoDelete=gNoDelete->next;            
		ExFreePoolWithTag(temp,'COMP');
		NoDelNum--;
	}
	else
	{
		for (temp=gNoDelete;temp!=NULL;tempbefore=temp,temp=temp->next)
		{
			if (!wcscmp(DelName,temp->Name))
			{
				tempbefore->next=temp->next;
				ExFreePoolWithTag(temp,'COMP');	
				NoDelNum--;
			}
		}
	}
	ExReleaseFastMutex(&NoDelMutex); 
	
}

//��ѯƥ�亯��  ��ֹɾ�����ֹ�����ж�Ҫ�������
BOOLEAN
SfCompareFullPath(
				  IN PCOMPATH HeadComPath,//��һ���������ڵ�ͷ
				  IN PFAST_MUTEX  Mutex,//�����Ӧ����
				  IN PWSTR  fullpath
				  )
{	
	PCOMPATH temp;
	if(!HeadComPath) 
	{
		return FALSE;
	}
	//KdPrint(("SfCompareFullPath   %s\n",HeadComPath->next));
	ExAcquireFastMutex( Mutex );
	for (temp=HeadComPath;temp!=NULL;temp=temp->next)
	{	
		if (!wcscmp(fullpath,temp->Name))
		{
			ExReleaseFastMutex(Mutex);
			return TRUE;
		}
	}
	ExReleaseFastMutex(Mutex);
	return FALSE;
}

//��ֹ���ʵ�����Ԫ�غ���
VOID
SfAddComPathHidAce(
				IN PWSTR Name//�����·����
				)
{
	PCOMPATH temp=gHidNoAccess,newComPath;
	newComPath= ExAllocatePoolWithTag(PagedPool, sizeof(COMPATH), 'COMP');
	ExAcquireFastMutex( &NoHidAceMutex );
	if (newComPath)
	{	
		gHidNoAccess=newComPath;//���µ�Ԫ��ת��Ϊ��ͷ
		wcscpy(gHidNoAccess->Name,Name);
		gHidNoAccess->next=temp;//ԭ����head�������
		NoHidAceNum++;
	}
	
	ExReleaseFastMutex(&NoHidAceMutex);
}
//��ֹ���ʵ�ɾ��Ԫ�غ���
VOID
SfDeleteComPathHidAce(
				   IN PWSTR DelName
				   )
{
	PCOMPATH temp;
    PCOMPATH tempbefore; 
	if (!gHidNoAccess)
    {
		return;
    }	
	ExAcquireFastMutex( &NoHidAceMutex );	
	if (!wcscmp(DelName,gHidNoAccess->Name))
	{
        temp=gHidNoAccess;
		gHidNoAccess=gHidNoAccess->next;            
		ExFreePoolWithTag(temp,'COMP');	
		NoHidAceNum--;
	}
    else
	{
		for (temp=gHidNoAccess;temp!=NULL;tempbefore=temp,temp=temp->next)
		{
			if (!wcscmp(DelName,temp->Name))
			{
				tempbefore->next=temp->next;
				ExFreePoolWithTag(temp,'COMP');	
				NoHidAceNum--;
			}
		}
	}
    ExReleaseFastMutex(&NoHidAceMutex); 
}


VOID AddReparse(PReparser NewReparse)
{
	PReparser temp=(PReparser)ExAllocatePoolWithTag(NonPagedPool,sizeof(Reparser),'Repa');
	wcscpy(temp->Sourcefile,NewReparse->Sourcefile);
	wcscpy(temp->Targetfile,NewReparse->Targetfile);
	ExAcquireFastMutex( &ReparseMutex );	
	InsertHeadList(&gReparseList,(PLIST_ENTRY)temp);
	ReparseNum++;
	ExReleaseFastMutex(&ReparseMutex); 
}

BOOLEAN DelReparse(PWSTR FileFullPath)
{		
	PLIST_ENTRY p;
	PReparser temp;
	ExAcquireFastMutex( &ReparseMutex );	
	for(p=gReparseList.Flink;p!=&gReparseList;p=p->Flink)
	{
		temp=(PReparser)p;
		if (!wcscmp(FileFullPath,temp->Sourcefile))
		{
			RemoveEntryList((PLIST_ENTRY)temp);
			ExReleaseFastMutex(&ReparseMutex); 
			ReparseNum--;
			ExFreePool(temp);
			return TRUE;
		}
	}
    ExReleaseFastMutex(&ReparseMutex);
	return FALSE;
}

BOOLEAN FindReparsePath(IN OUT PWSTR FileFullPath)
{
	PLIST_ENTRY p;
	PReparser temp;
	ExAcquireFastMutex( &ReparseMutex );	
	for(p=gReparseList.Flink;p!=&gReparseList;p=p->Flink)
	{
		temp=(PReparser)p;
		if (!wcscmp(FileFullPath,temp->Sourcefile))
		{
		    wcscpy(FileFullPath,temp->Targetfile);    
			ExReleaseFastMutex(&ReparseMutex);
			return TRUE;
		}
	}    
	ExReleaseFastMutex(&ReparseMutex);
	return FALSE;		
}
