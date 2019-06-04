#include "ntifs.h"
#include "sfilter.h"
#include "Ioctlcmd.h"
#include "hash.c"
//#include "list.c"
#include "hide.c"
#include "fastio.c"
#include "crypt.c"


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#if DBG
#pragma alloc_text(PAGE, DriverUnload)
#endif
#pragma alloc_text(PAGE, SfFsNotification)
#pragma alloc_text(PAGE, SfCreate)
#pragma alloc_text(PAGE, SfRead)
#pragma alloc_text(PAGE, SfWrite)
#pragma alloc_text(PAGE, SfClose)
#pragma alloc_text(PAGE, SfCleanup)
#pragma alloc_text(PAGE, SfDirectoryControl)
#pragma alloc_text(PAGE, SfSetInformation)
#pragma alloc_text(PAGE, SfFsControl)

#pragma alloc_text(PAGE, SfFsControlMountVolume)
#pragma alloc_text(PAGE, SfFsControlMountVolumeComplete)
#pragma alloc_text(PAGE, SfFsControlLoadFileSystem)
#pragma alloc_text(PAGE, SfFsControlLoadFileSystemComplete)

#pragma alloc_text(PAGE, SfAttachDeviceToDeviceStack)
#pragma alloc_text(PAGE, SfAttachToFileSystemDevice)
#pragma alloc_text(PAGE, SfDetachFromFileSystemDevice)
#pragma alloc_text(PAGE, SfAttachToMountedDevice)
#pragma alloc_text(PAGE, SfIsAttachedToDevice)

#pragma alloc_text(PAGE, SfIsShadowCopyVolume)
#pragma alloc_text(INIT, SfLoadDynamicFunctions)
#pragma alloc_text(INIT, SfGetCurrentVersion)
#pragma alloc_text(PAGE, SfEnumerateFileSystemVolumes)


#endif


//�������ڿ����Ƿ����ض���ı�־��  Ĭ��Ϊ������
BOOLEAN bRedirectFileOpen=TRUE;


NTSTATUS
DriverEntry (
	     IN PDRIVER_OBJECT DriverObject,
	     IN PUNICODE_STRING RegistryPath
	     )
{
    PFAST_IO_DISPATCH fastIoDispatch;
	UNICODE_STRING nameString;
	NTSTATUS status;
	ULONG i;
	UNICODE_STRING LinkName;	

	//����������ľ���   ��ʱ�ò���ע��·��
	UNREFERENCED_PARAMETER(RegistryPath);
	
	//
	//��̬����
	//
	SfLoadDynamicFunctions();

	//
	//��õ�ǰ����ϵͳ�汾
	//
	SfGetCurrentVersion();

	//
	//�����豸����
	//
	gSFilterDriverObject = DriverObject;

	//
	//�ڵ���ģʽ�� ����ж������
	//
	#if DBG 
	if (NULL != gSfDynamicFunctions.EnumerateDeviceObjectList) 
	{        
		gSFilterDriverObject->DriverUnload = DriverUnload;
	}
	#endif

	//
	//��ʼ��Mutex
	//
	
	ExInitializeFastMutex( &ReparseMutex );
	ExInitializeFastMutex( &NoDelMutex );	
	ExInitializeFastMutex( &NoAceMutex );
	ExInitializeFastMutex( &NoHidAceMutex );
	
	//��ʼ���ļ����ʿ��Ƶ�ȫ�����ݿ�
	gNoDelete=NULL;
	gNoAccess=NULL;
	gHidNoAccess=NULL;
	InitializeListHead(&gReparseList);

	NoAceNum = 0;
	NoDelNum = 0;
	NoHidAceNum=0;
	ReparseNum=0;
	/////////////////////////////////////͸�������еĳ�ʼ��////////////////////////////
    
	cfCurProcNameInit();               //�ҳ���������ƫ��
	cfListInit();                      //��ʼ���ļ���������
	
	///////////////////////////////////////////////////////////////////////////////////
	//
	//hash��ʼ��һ����Դ����
	//
	ExInitializeResourceLite( &HashResource );
	
	//
	//��ʼ�������ļ����ص�LIST_ENTRY
	//
	
	InitializeListHead(&g_HideObjHead);
	//
	//��ʼ���󶨾�Ҫ�õĿ��ٻ�����
	//

	ExInitializeFastMutex( &gSfilterAttachLock );



	RtlInitUnicodeString( &nameString, L"\\FileSystem\\Filters\\SFilter" );//��ʼ��UNICODE�ַ���
	
	status = IoCreateDevice( DriverObject,	//�����豸����
			0,                      //���豸��չ
			&nameString,		//�����豸���������
			FILE_DEVICE_DISK_FILE_SYSTEM,//�����ļ�ϵͳ�豸����
			FILE_DEVICE_SECURE_OPEN,//����FILE_DEVICE_SECURE_OPEN ���Է�ֹǱ�ڵİ�ȫ©��
			FALSE,			//�ö��󲻿����ں�ģʽ��ʹ��
			&gSFilterControlDeviceObject //�������صĶ����ַ����˱�����
			);

	if (!NT_SUCCESS( status ))	//CDOʧ��
	{
		KdPrint(( "SFilter!DriverEntry: Error creating control device object \"%wZ\", status=%08x\n", &nameString, status ));
		return status;
	}

	
	//
	//������������
	//

	RtlInitUnicodeString( &LinkName, DOS_DEVICE_NAME);

	status = IoCreateSymbolicLink( &LinkName, &nameString );
	if (!NT_SUCCESS( status ))
	{//������������ ʧ�ܣ�
		KdPrint(( "SFilter!DriverEntry: IoCreateSymbolicLink failed\n"));		
		IoDeleteDevice(gSFilterControlDeviceObject);
		return status;	
	}


	//
	//Ĭ��IRP��ʼ�ַ�
	//

	for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		DriverObject->MajorFunction[i] = SfPassThrough;
	}

	DriverObject->MajorFunction[IRP_MJ_CREATE] = 
	DriverObject->MajorFunction[IRP_MJ_CREATE_NAMED_PIPE] = 
	DriverObject->MajorFunction[IRP_MJ_CREATE_MAILSLOT] = SfCreate;
	DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL] = SfFsControl;
	DriverObject->MajorFunction[IRP_MJ_CLEANUP] = SfCleanup;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = SfClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]=SfDeviceIOControl;
	DriverObject->MajorFunction[IRP_MJ_DIRECTORY_CONTROL] = SfDirectoryControl;//�������IRP�����ļ�����
	DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION] = SfSetInformation;
	DriverObject->MajorFunction[IRP_MJ_READ] = SfRead;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = SfWrite;
	DriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION] = SfQueryInformation;


	fastIoDispatch = ExAllocatePoolWithTag( NonPagedPool, sizeof( FAST_IO_DISPATCH ), SFLT_POOL_TAG );
	
	//
	//�����ڴ��Ƿ�ɹ�
	//
	if (!fastIoDispatch) 
	{
		
		IoDeleteDevice( gSFilterControlDeviceObject );
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	
	RtlZeroMemory( fastIoDispatch, sizeof( FAST_IO_DISPATCH ) );//�ڴ�����
	fastIoDispatch->SizeOfFastIoDispatch = sizeof( FAST_IO_DISPATCH );
	fastIoDispatch->FastIoCheckIfPossible = SfFastIoCheckIfPossible;
	fastIoDispatch->FastIoRead = SfFastIoRead;
	fastIoDispatch->FastIoWrite = SfFastIoWrite;
	fastIoDispatch->FastIoQueryBasicInfo = SfFastIoQueryBasicInfo;
	fastIoDispatch->FastIoQueryStandardInfo = SfFastIoQueryStandardInfo;
	fastIoDispatch->FastIoLock = SfFastIoLock;
	fastIoDispatch->FastIoUnlockSingle = SfFastIoUnlockSingle;
	fastIoDispatch->FastIoUnlockAll = SfFastIoUnlockAll;
	fastIoDispatch->FastIoUnlockAllByKey = SfFastIoUnlockAllByKey;
	fastIoDispatch->FastIoDeviceControl = SfFastIoDeviceControl;
	fastIoDispatch->FastIoDetachDevice = SfFastIoDetachDevice;
	fastIoDispatch->FastIoQueryNetworkOpenInfo = SfFastIoQueryNetworkOpenInfo;
	fastIoDispatch->MdlRead = SfFastIoMdlRead;
	fastIoDispatch->MdlReadComplete = SfFastIoMdlReadComplete;
	fastIoDispatch->PrepareMdlWrite = SfFastIoPrepareMdlWrite;
	fastIoDispatch->MdlWriteComplete = SfFastIoMdlWriteComplete;
	fastIoDispatch->FastIoReadCompressed = SfFastIoReadCompressed;
	fastIoDispatch->FastIoWriteCompressed = SfFastIoWriteCompressed;
	fastIoDispatch->MdlReadCompleteCompressed = SfFastIoMdlReadCompleteCompressed;
	fastIoDispatch->MdlWriteCompleteCompressed = SfFastIoMdlWriteCompleteCompressed;
	fastIoDispatch->FastIoQueryOpen = SfFastIoQueryOpen;
	
	DriverObject->FastIoDispatch = fastIoDispatch;
	{
		FS_FILTER_CALLBACKS fsFilterCallbacks;//����ṹ��WDK����˵��
		if (NULL != gSfDynamicFunctions.RegisterFileSystemFilterCallbacks) {
			//����ֻ�Ǽ򵥸���һ����һ�����������������̵�����
			fsFilterCallbacks.SizeOfFsFilterCallbacks = sizeof( FS_FILTER_CALLBACKS );
			fsFilterCallbacks.PreAcquireForSectionSynchronization = SfPreFsFilterPassThrough;
			fsFilterCallbacks.PostAcquireForSectionSynchronization = SfPostFsFilterPassThrough;
			fsFilterCallbacks.PreReleaseForSectionSynchronization = SfPreFsFilterPassThrough;
			fsFilterCallbacks.PostReleaseForSectionSynchronization = SfPostFsFilterPassThrough;
			fsFilterCallbacks.PreAcquireForCcFlush = SfPreFsFilterPassThrough;
			fsFilterCallbacks.PostAcquireForCcFlush = SfPostFsFilterPassThrough;
			fsFilterCallbacks.PreReleaseForCcFlush = SfPreFsFilterPassThrough;
			fsFilterCallbacks.PostReleaseForCcFlush = SfPostFsFilterPassThrough;
			fsFilterCallbacks.PreAcquireForModifiedPageWriter = SfPreFsFilterPassThrough;
			fsFilterCallbacks.PostAcquireForModifiedPageWriter = SfPostFsFilterPassThrough;
			fsFilterCallbacks.PreReleaseForModifiedPageWriter = SfPreFsFilterPassThrough;
			fsFilterCallbacks.PostReleaseForModifiedPageWriter = SfPostFsFilterPassThrough;
			status = (gSfDynamicFunctions.RegisterFileSystemFilterCallbacks)( DriverObject, //_SF_DYNAMIC_FUNCTION_POINTERS�ṹ��ĵ�һ��������ʹ��
				&fsFilterCallbacks );//֪ͨ�ص�����������
		
			//
			//��ʧ�ܾ��ͷ��ڴ沢ɾ����������
			//
			if (!NT_SUCCESS( status )) 
			{ 
				               
				DriverObject->FastIoDispatch = NULL;
				ExFreePool( fastIoDispatch );
				IoDeleteDevice( gSFilterControlDeviceObject );
				return status;
			}
		}
	}


	//
	//ע��ص�����
	//
	status = IoRegisterFsRegistrationChange( DriverObject, SfFsNotification );
	
	if (!NT_SUCCESS( status )) 
	{
		KdPrint(( "SFilter!DriverEntry: Error registering FS change notification, status=%08x\n", status ));
		DriverObject->FastIoDispatch = NULL;
		ExFreePool( fastIoDispatch );
		IoDeleteDevice( gSFilterControlDeviceObject );
		return status;
	}


	//
	//�������
	//���DO_DEVICE_INITIALIZING��ǣ�
	//

	ClearFlag( gSFilterControlDeviceObject->Flags, DO_DEVICE_INITIALIZING );
	
	return STATUS_SUCCESS;
}//driverentry����


//ж�غ���
#if DBG
VOID
DriverUnload (   
	      IN PDRIVER_OBJECT DriverObject
	      )
{
	//
	//���豸��չ�йص�һ���ṹ��  
	//

	PSFILTER_DEVICE_EXTENSION devExt;

	PFAST_IO_DISPATCH fastIoDispatch;

	NTSTATUS status;
	ULONG numDevices;//�豸������
	ULONG i;
	LARGE_INTEGER interval;//64λ��������
	UNICODE_STRING LinkName;

	#define DEVOBJ_LIST_SIZE 64
	PDEVICE_OBJECT devList[DEVOBJ_LIST_SIZE];

	//*****************************************************************************
	PLIST_ENTRY pdLink = NULL;
	PHIDE_FILE  pHideObj = NULL;
	PHIDE_DIRECTOR pHideDir = NULL;
	PLIST_ENTRY pdLinkDir = NULL;
	PLIST_ENTRY HeadList;
	//******************************************************************************

	//
	//ɾ����������
	//
	RtlInitUnicodeString( &LinkName, DOS_DEVICE_NAME);	
	IoDeleteSymbolicLink(&LinkName );
	
	//////////////////////////////////////////////////////////////////////////////////////////
	ASSERT(DriverObject == gSFilterDriverObject);

	//
	//�����ٻ���κε��ļ�ϵͳ����Ϣ��  
	//����ص�����
	//

	IoUnregisterFsRegistrationChange( DriverObject, SfFsNotification );

	//
	//��ѭ�� ֱ��numDevices <= 0ʱ����
	//��ѭ����������и������豸����
	//
	for (;;) 
	{
		ASSERT( NULL != gSfDynamicFunctions.EnumerateDeviceObjectList );
		status = (gSfDynamicFunctions.EnumerateDeviceObjectList)(
                        DriverObject,
                        devList,
                        sizeof(devList),
                        &numDevices);

		if (numDevices <= 0) 
		{
			break;
		}
		numDevices = min( numDevices, DEVOBJ_LIST_SIZE );

		//
		//���ȱ����б����ÿ���豸
		//CDOû���豸��չ���ð��κζ��������� ���Բ��ð���
		//

		for (i=0; i < numDevices; i++) 
		{
			devExt = devList[i]->DeviceExtension;
			if (NULL != devExt) 
			{
				IoDetachDevice( devExt->AttachedToDeviceObject );
			}
		}

		//
		//�ȴ�5��
		//�õ�ǰIRP���
		//
		
		interval.QuadPart = (5 * DELAY_ONE_SECOND);      
		
		KeDelayExecutionThread( KernelMode, FALSE, &interval );
		
		//
		//�ص��豸��  ɾ���豸����
		//

		for (i=0; i < numDevices; i++) 
		{
			//
			//�豸��չΪ�յľ������ǵ�CDO
			//

			if (NULL != devList[i]->DeviceExtension) 
			{
				
				SfCleanupMountedDevice( devList[i] );
			}
			else
			{
				ASSERT(devList[i] == gSFilterControlDeviceObject);
				gSFilterControlDeviceObject = NULL;
			}
			
			//
			//ɾ���豸
			//
			IoDeleteDevice( devList[i] );

			//
			//���ټ�����IoEnumerateDeviceObjectList�����Ӽ�����
			//

			ObDereferenceObject( devList[i] );
		}
	}//��ѭ������

	//
	//�ͷ�fastio��  �ڴ��ͷ�
	//

	fastIoDispatch = DriverObject->FastIoDispatch;
	DriverObject->FastIoDispatch = NULL;
	
	//
	//�ͷ�hashtable
	//
	SfHashCleanup();

	/////////////////////////////////////////
	//
	//�ͷ������ļ�����
	//////////////////////////////�������޸ĵ�ж�غ���///////////
	while(!IsListEmpty(&g_HideObjHead))
	{
		pdLinkDir = RemoveHeadList(&g_HideObjHead);
		pHideDir = CONTAINING_RECORD(pdLinkDir, HIDE_DIRECTOR, linkfield);
		HeadList = &(pHideDir->link);
		while (!IsListEmpty(HeadList))
		{
			pdLink = RemoveHeadList(HeadList);
			pHideObj = CONTAINING_RECORD(pdLink, HIDE_FILE, linkfield);
			ExFreePool(pHideObj);
		}
		ExFreePool(pHideDir);
	}	
	///////////////////////////////////////////////////////////////////////////////////

	
	
}//DriverUnload����


#endif//#if DBG



//
//�˺������Գ��Լ��ز�ͬ�汾��OS�ĺ���ָ��
//
VOID
SfLoadDynamicFunctions (
			)
{
	UNICODE_STRING functionName;
	
	RtlZeroMemory( &gSfDynamicFunctions, sizeof( gSfDynamicFunctions ) );
	RtlInitUnicodeString( &functionName, L"FsRtlRegisterFileSystemFilterCallbacks" );
	gSfDynamicFunctions.RegisterFileSystemFilterCallbacks = MmGetSystemRoutineAddress( &functionName );
	RtlInitUnicodeString( &functionName, L"IoAttachDeviceToDeviceStackSafe" );
	gSfDynamicFunctions.AttachDeviceToDeviceStackSafe = MmGetSystemRoutineAddress( &functionName );
	RtlInitUnicodeString( &functionName, L"IoEnumerateDeviceObjectList" );
	gSfDynamicFunctions.EnumerateDeviceObjectList = MmGetSystemRoutineAddress( &functionName );
	RtlInitUnicodeString( &functionName, L"IoGetLowerDeviceObject" );
	gSfDynamicFunctions.GetLowerDeviceObject = MmGetSystemRoutineAddress( &functionName );
	RtlInitUnicodeString( &functionName, L"IoGetDeviceAttachmentBaseRef" );
	gSfDynamicFunctions.GetDeviceAttachmentBaseRef = MmGetSystemRoutineAddress( &functionName );
	RtlInitUnicodeString( &functionName, L"IoGetDiskDeviceObject" );
	gSfDynamicFunctions.GetDiskDeviceObject = MmGetSystemRoutineAddress( &functionName );
	RtlInitUnicodeString( &functionName, L"IoGetAttachedDeviceReference" );
	gSfDynamicFunctions.GetAttachedDeviceReference = MmGetSystemRoutineAddress( &functionName );
	RtlInitUnicodeString( &functionName, L"RtlGetVersion" );
	gSfDynamicFunctions.GetVersion = MmGetSystemRoutineAddress( &functionName ); 
	
}

//
//���OS�汾
//
VOID
SfGetCurrentVersion (  
		     )
{
	
	if (NULL != gSfDynamicFunctions.GetVersion) 
	{
		//ֱ��assert����
		RTL_OSVERSIONINFOW versionInfo;
		NTSTATUS status;
		versionInfo.dwOSVersionInfoSize = sizeof( RTL_OSVERSIONINFOW );
		status = (gSfDynamicFunctions.GetVersion)( &versionInfo );
		ASSERT( NT_SUCCESS( status ) );
		gSfOsMajorVersion = versionInfo.dwMajorVersion;
		gSfOsMinorVersion = versionInfo.dwMinorVersion;        
	} 
	else 
	{
		PsGetVersion( &gSfOsMajorVersion,
			&gSfOsMinorVersion,
			NULL,
			NULL );
	}
	
}


//
//�ص�����,���ļ�ϵͳ��������߳���ʱ����  
//�ڸ�������,��ɶ��ļ�ϵͳ�����豸����İ�.
//

// ������̴���һ���豸���������ӵ�ָ�����ļ�ϵͳ�����豸����
// �Ķ���ջ��,�����������豸����������з��͸��ļ�ϵͳ������.
// ����,���Ǿ��ܻ��һ�����ؾ������,�Ϳ��Ը��ӵ�����µľ��豸����
// ���豸����ջ��
// DeviceObject: ָ�򱻼�����߳������ļ�ϵͳ�Ŀ����豸����
// FsActive: ������߳�����
//
VOID
SfFsNotification (
		  IN PDEVICE_OBJECT DeviceObject,
		  IN BOOLEAN FsActive
		  )
		 
{
	UNICODE_STRING name;	//�ļ����ƶ�����豸��
	WCHAR nameBuffer[MAX_DEVNAME_LENGTH];
	PAGED_CODE();
	
	//
	//�ڴ����
	//

	RtlInitEmptyUnicodeString( &name, nameBuffer, sizeof(nameBuffer) );
	
	SfGetObjectName( DeviceObject, &name );
	

	//
	//���� �󶨻��߽�󣨴��������ļ�ϵͳ�У�
	//

	if (FsActive) 
	{
		
		SfAttachToFileSystemDevice( DeviceObject, &name );
	} 
	else 
	{		
		
		SfDetachFromFileSystemDevice( DeviceObject );
	}
	
}

///////////////////////////////////////////////////////////////////////////
/////Write  ���ܴ���
///////////////////////////////////////////////////////////////////////////
NTSTATUS
SfWrite(
		IN PDEVICE_OBJECT DeviceObject,
		IN PIRP Irp
		)
{
	PIO_STACK_LOCATION currentIrpStack;
	PFILE_OBJECT FileObject;
	NTSTATUS status;
	KEVENT waitEvent;
	PVOID context;                   //д�����Ǳ���������ָ�룬�����������ݲ�����
	
	
	BOOLEAN proc_sec = cfIsCurProcSec();    //�жϵ�ǰ�Ƿ��Ǽ��ܽ���
    BOOLEAN crypting;
	
	ASSERT(!IS_MY_CONTROL_DEVICE_OBJECT( DeviceObject ));
    ASSERT(IS_MY_DEVICE_OBJECT( DeviceObject ));
	
	//��õ�ǰIRPջ  
    currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
	FileObject = currentIrpStack->FileObject;
	
	/////////////////////////////////////////͸������///////////////////////
	// �Ƿ���һ���Ѿ������ܽ��̴򿪵��ļ�
	
	if(!cfListInited())
	{
		IoSkipCurrentIrpStackLocation(Irp);
		return IoCallDriver( ((PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp );
	}
	
    cfListLock();
    crypting = cfIsFileCrypting(currentIrpStack->FileObject);
    cfListUnlock();
	
	if (proc_sec&&crypting&&(Irp->Flags & (IRP_PAGING_IO|IRP_SYNCHRONOUS_PAGING_IO|IRP_NOCACHE)))
	{
		if (cfIrpWritePre(Irp,currentIrpStack,&context))
		{
			PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
			//�ȴ��¼����ڵȴ��������
			KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );
			//������ǰI/O��ջ����һ����ջ���������ǵ��������
			IoCopyCurrentIrpStackLocationToNext( Irp );
			IoSetCompletionRoutine(
				Irp,
				SfWriteCompletion,//�������
				&waitEvent,
				TRUE,
				TRUE,
				TRUE );
			//����ջ�е���һ������
			status = IoCallDriver( ((PSFILTER_DEVICE_EXTENSION) DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp );
			//�ȴ��ں��¼�
			if (STATUS_PENDING == status) {
				NTSTATUS localStatus = KeWaitForSingleObject(	&waitEvent, 
					Executive,//�ȴ���ԭ��
					KernelMode,//�������������
					FALSE,
					NULL//���޵ĵ���ȥ
					);
				ASSERT(STATUS_SUCCESS == localStatus);
			}
			//��֤IoCompleteRequest��������
			ASSERT(KeReadStateEvent(&waitEvent) ||!NT_SUCCESS(Irp->IoStatus.Status));		
			///////////////////////////////////////	
			//���ܴ���
			
			ASSERT(crypting);
			
            cfIrpWritePost(Irp,irpSp,context);
			
			//////////////////////////////////////////////////////////////////////////
			status = Irp->IoStatus.Status;
			IoCompleteRequest( Irp, IO_NO_INCREMENT );
            return status;
		} 
		else
		{
			status = Irp->IoStatus.Status;
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return status;
		}
		
	}
	
	IoSkipCurrentIrpStackLocation( Irp );
    return IoCallDriver( ((PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp );
}

NTSTATUS
SfWriteCompletion (
					IN PDEVICE_OBJECT DeviceObject,
					IN PIRP Irp,
					IN PVOID Context
					)
					//������� �������Ե��� ��ô����Ĺ��ܾ��Ǵ�ӡ�ɹ����ļ�ϵͳ�򿪵��ļ����ļ���
{
    PKEVENT event = Context;
    UNREFERENCED_PARAMETER( DeviceObject );
    UNREFERENCED_PARAMETER( Irp );
    ASSERT(IS_MY_DEVICE_OBJECT( DeviceObject ));
    KeSetEvent(event, IO_NO_INCREMENT, FALSE);//���õȴ��¼�
    return STATUS_MORE_PROCESSING_REQUIRED;//������Ҫ��һ������
}

///////////////////////////////////////////////////////////////////////////
/////Read  ���ܴ���
//////////////////////////////////////////////////////////////////////////
NTSTATUS
SfRead(
	   IN PDEVICE_OBJECT DeviceObject,
	   IN PIRP Irp
	   )
{
	PIO_STACK_LOCATION currentIrpStack;
	PFILE_OBJECT FileObject;
	NTSTATUS status;
	KEVENT waitEvent; 
	//����û�б�Ҫ����ʱ�Ȳ���
	//BOOLEAN proc_sec = cfIsCurProcSec();    //�жϵ�ǰ�Ƿ��Ǽ��ܽ���
    BOOLEAN crypting;
	
	ASSERT(!IS_MY_CONTROL_DEVICE_OBJECT( DeviceObject ));
    ASSERT(IS_MY_DEVICE_OBJECT( DeviceObject ));
	
	//��õ�ǰIRPջ  
    currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
	FileObject = currentIrpStack->FileObject;
    
    /////////////////////////////////////͸������////////////////////////////////
	//Ӧ�ñ�֤�����Ķ����Ǽ��ܽ������Ǽ����ļ������������������·�
	
	if(!cfListInited())
	{
		IoSkipCurrentIrpStackLocation(Irp);
		return IoCallDriver( ((PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp );
	}
	
	// �Ƿ���һ���Ѿ������ܽ��̴򿪵��ļ�
    cfListLock();
    crypting = cfIsFileCrypting(currentIrpStack->FileObject);
    cfListUnlock();
	
	if(crypting&&(Irp->Flags & (IRP_PAGING_IO|IRP_SYNCHRONOUS_PAGING_IO|IRP_NOCACHE)))
	{
		PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
		//ȷ�����Ƿ���Ҫ�������д����Ԥ����
		cfIrpReadPre(Irp,currentIrpStack);
		
        //���������������¼��ȴ���д�����������
		//�ȴ��¼����ڵȴ��������
		KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );
		//������ǰI/O��ջ����һ����ջ���������ǵ��������
		IoCopyCurrentIrpStackLocationToNext( Irp );
		IoSetCompletionRoutine(
			Irp,
			SfReadCompletion,//�������
			&waitEvent,
			TRUE,
			TRUE,
			TRUE );
		//����ջ�е���һ������
		status = IoCallDriver( ((PSFILTER_DEVICE_EXTENSION) DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp );
		//�ȴ��ں��¼�
		if (STATUS_PENDING == status) {
			NTSTATUS localStatus = KeWaitForSingleObject(	&waitEvent, 
				Executive,//�ȴ���ԭ��
				KernelMode,//�������������
				FALSE,
				NULL//���޵ĵ���ȥ
				);
			ASSERT(STATUS_SUCCESS == localStatus);
		}
		//��֤IoCompleteRequest��������
		ASSERT(KeReadStateEvent(&waitEvent) ||!NT_SUCCESS(Irp->IoStatus.Status));		
		//////////////////////////////////////////////////////////////////////////
		//���ܴ���
		ASSERT(crypting);
		
        cfIrpReadPost(Irp,irpSp);
		
		//////////////////////////////////////////////////////////////////////////
		status = Irp->IoStatus.Status;
		IoCompleteRequest( Irp, IO_NO_INCREMENT );
		return status;
	}
	
    IoSkipCurrentIrpStackLocation( Irp );
    return IoCallDriver( ((PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp );
}
NTSTATUS
SfReadCompletion (
				   IN PDEVICE_OBJECT DeviceObject,
				   IN PIRP Irp,
				   IN PVOID Context
				   )
				   //������� �������Ե��� ��ô����Ĺ��ܾ��Ǵ�ӡ�ɹ����ļ�ϵͳ�򿪵��ļ����ļ���
{
    PKEVENT event = Context;
    UNREFERENCED_PARAMETER( DeviceObject );
    UNREFERENCED_PARAMETER( Irp );
    ASSERT(IS_MY_DEVICE_OBJECT( DeviceObject ));
    KeSetEvent(event, IO_NO_INCREMENT, FALSE);//���õȴ��¼�
    return STATUS_MORE_PROCESSING_REQUIRED;//������Ҫ��һ������
}

NTSTATUS
SfQueryInformation(
				   IN PDEVICE_OBJECT DeviceObject,
				   IN PIRP Irp
				   )
{
	NTSTATUS status;
	PIO_STACK_LOCATION  irpSp= IoGetCurrentIrpStackLocation(Irp);	//��ǰIrp(IO_STACK_LOCATION)�Ĳ���
	PSFILTER_DEVICE_EXTENSION devExt = DeviceObject->DeviceExtension;
	PFILE_BOTH_DIR_INFORMATION dirInfo = NULL;
	KEVENT waitEvent;
	BOOLEAN crypting;           //�ж�����ļ��Ƿ��ڼ��ܱ���
	
	
	ASSERT(gSFilterControlDeviceObject != DeviceObject);
	ASSERT(IS_MY_DEVICE_OBJECT(DeviceObject));
	
	//��Ȼ����Ҫ�õ������������⻹û��ʼ���ã���ֱ���·�
	if(!cfListInited())
	{
		IoSkipCurrentIrpStackLocation(Irp);
		return IoCallDriver(devExt->AttachedToDeviceObject, Irp);
	}
	////////////////////////////////////////////////////////////////////
	// �Ƿ���һ���Ѿ������ܽ��̴򿪵��ļ�
    cfListLock();
    // �����create,����Ҫ�ָ��ļ����ȡ����������������pre��
    // ʱ���Ӧ���Ѿ��ָ��ˡ�
    crypting = cfIsFileCrypting(irpSp->FileObject);
    cfListUnlock();
	
	if (crypting && (irpSp->Parameters.QueryFile.FileInformationClass == FileAllInformation ||
		irpSp->Parameters.QueryFile.FileInformationClass == FileAllocationInformation ||
		irpSp->Parameters.QueryFile.FileInformationClass == FileEndOfFileInformation ||
		irpSp->Parameters.QueryFile.FileInformationClass == FileStandardInformation ||
		irpSp->Parameters.QueryFile.FileInformationClass == FilePositionInformation ||
        irpSp->Parameters.QueryFile.FileInformationClass == FileValidDataLengthInformation))
	{
		//������ɻص�����  �����֪���Ժ�᲻��ʹ�õ�   ����������
		KeInitializeEvent(&waitEvent, NotificationEvent, FALSE);
		IoCopyCurrentIrpStackLocationToNext(Irp);
		
		IoSetCompletionRoutine(	
			Irp,
			SfQueryInformationCompletion,		//CompletionRoutine
			&waitEvent,					//context parameter
			TRUE,
			TRUE,
			TRUE
			);	
		status = IoCallDriver(devExt->AttachedToDeviceObject, Irp);
		
		if (STATUS_PENDING == status)
		{
			//�ȴ����
			status = KeWaitForSingleObject(&waitEvent,
				Executive,
				KernelMode,
				FALSE,
				NULL
				);
			ASSERT(STATUS_SUCCESS == status);
		}
		ASSERT(KeReadStateEvent(&waitEvent) ||
			!NT_SUCCESS(Irp->IoStatus.Status));
		
		ASSERT(crypting);
        cfIrpQueryInforPost(Irp,irpSp);
		
		status = Irp->IoStatus.Status;
        IoCompleteRequest( Irp, IO_NO_INCREMENT );
		
		return status;
	}else{
		//�������ֱ���·�
		IoSkipCurrentIrpStackLocation(Irp);
		return IoCallDriver(devExt->AttachedToDeviceObject, Irp);
	}
}

NTSTATUS
SfQueryInformationCompletion(
							 IN PDEVICE_OBJECT DeviceObject,
							 IN PIRP Irp,
							 IN PVOID Context
							 )
{
	PKEVENT event = Context;
	UNREFERENCED_PARAMETER( Irp );
	UNREFERENCED_PARAMETER( DeviceObject );
	KeSetEvent(event, IO_NO_INCREMENT, FALSE);
	return STATUS_MORE_PROCESSING_REQUIRED;	//ע�����뷵�����ֵ
}

///////////////////////////////////////////////////////////////////////////
/////�ļ�����             IRP��������//////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
NTSTATUS
SfDirectoryControl(
		   IN PDEVICE_OBJECT DeviceObject,
		   IN PIRP Irp
		   )
{
	NTSTATUS status;

	PLIST_ENTRY headListEntry = &g_HideObjHead;
	PLIST_ENTRY tmpListEntry = headListEntry;
	PHIDE_DIRECTOR temHideDir = NULL;

	PIO_STACK_LOCATION  irpSp= IoGetCurrentIrpStackLocation(Irp);	
	PSFILTER_DEVICE_EXTENSION devExt = DeviceObject->DeviceExtension;
	PFILE_BOTH_DIR_INFORMATION dirInfo = NULL;
	KEVENT waitEvent;

	ASSERT(gSFilterControlDeviceObject != DeviceObject);

	ASSERT(IS_MY_DEVICE_OBJECT(DeviceObject));

	//
	//�ж�IRP���汾�� �Ƿ�Ϊ IRP_MN_QUERY_DIRECTORY
	//
	if (IRP_MN_QUERY_DIRECTORY != irpSp->MinorFunction)
	{
		goto SkipHandle;
	}
	//
	//IRP ��ͷ�� RequestorMode ֵ��ȷ�� I/O ��������
	//�ں�ģʽ�����û�ģʽ����
	//
	if (Irp->RequestorMode == KernelMode)
	{
		goto SkipHandle;
	}
	//
	//FileInformationClass �Ƿ�Ϊ FileBothDirectoryInformation
	//FileBothDirectoryInformation ������
	//FileDispositionInformation   ��ɾ��
	//
	if (FileBothDirectoryInformation != ((PQUERY_DIRECTORY)&irpSp->Parameters)->FileInformationClass) 
	{	
		goto SkipHandle;
	}

	//
	//������ɻص�����,��ʼ��һ���¼�
	//

	KeInitializeEvent(&waitEvent, NotificationEvent, FALSE);

	//
	//����IRP��ջ
	//

	IoCopyCurrentIrpStackLocationToNext(Irp);

	//
	//IoCompleteRequest������ ���� ���ε����ϲ�������������
	//��������ĳ��IoCompletion���̷�����STATUS_MORE_PROCESSING_REQUIRED
	//

	IoSetCompletionRoutine(	
		Irp,
		SfDirectoryControlCompletion,		//CompletionRoutine
		&waitEvent,				//context parameter
		TRUE,
		TRUE,
		TRUE
		);
	
	status = IoCallDriver(devExt->AttachedToDeviceObject, Irp);

	if (STATUS_PENDING == status)
	{
		//
		//�ȴ����
		//

		status = KeWaitForSingleObject(&waitEvent,
			Executive,
			KernelMode,
			FALSE,
			NULL
			);
		ASSERT(STATUS_SUCCESS == status);
	}

	//
	//�������IRP����  IoCallDriverʧ�ܻ���userbuffer��û����Ϣ
	//

	if (!NT_SUCCESS(status) ||(0 == irpSp->Parameters.QueryFile.Length)) 
	{	
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}
	
//��������ص���Ҫ���� ��Ҫ��д   �ص��о���Irp->UserBuffer�е�����
 	{
		WCHAR  fullpath[1024];
		if(SfGetFullPath(irpSp->FileObject, fullpath))
		{	
		    while (tmpListEntry->Flink != headListEntry)//����������Ŀ¼�б�
			{
				//�������б�ȡ���������е�ֵ
				tmpListEntry = tmpListEntry->Flink;
				temHideDir = (PHIDE_DIRECTOR)CONTAINING_RECORD(tmpListEntry, HIDE_DIRECTOR, linkfield);
				//tmpHideFile = (PHIDE_FILE)CONTAINING_RECORD((temHideDir->link.Flink), HIDE_FILE, linkfield);
				
				if (!wcscmp(temHideDir->fatherPath,fullpath))
				{
					HandleDirectory(Irp->UserBuffer,  &((PQUERY_DIRECTORY)&irpSp->Parameters)->Length,temHideDir);
					break;
				}
			}
		}
		
 	}

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
	
SkipHandle:
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(devExt->AttachedToDeviceObject, Irp);

}



NTSTATUS
SfDirectoryControlCompletion(
			     IN PDEVICE_OBJECT DeviceObject,
			     IN PIRP Irp,
			     IN PVOID Context
			     )
{
	UNREFERENCED_PARAMETER( Irp );
	UNREFERENCED_PARAMETER( DeviceObject );



	KeSetEvent((PKEVENT)Context, IO_NO_INCREMENT, FALSE);
	return STATUS_MORE_PROCESSING_REQUIRED;	//ע�����뷵�����ֵ
}


///////////////////////////////////////////////////////////////////////////
/////��ֹɾ��             IRP��������//////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
NTSTATUS
SfSetInformation(
				 IN PDEVICE_OBJECT DeviceObject,
				 IN PIRP Irp
				 )
{
//	NTSTATUS status;
	PIO_STACK_LOCATION  irpSp= IoGetCurrentIrpStackLocation(Irp);	//��ǰIrp(IO_STACK_LOCATION)�Ĳ���
	PSFILTER_DEVICE_EXTENSION devExt = DeviceObject->DeviceExtension;
	PFILE_BOTH_DIR_INFORMATION dirInfo = NULL;
//	KEVENT waitEvent;
	BOOLEAN file_sec;
	WCHAR fullpath[1024];

	ASSERT(gSFilterControlDeviceObject != DeviceObject);
	ASSERT(IS_MY_DEVICE_OBJECT(DeviceObject));

	if(!cfListInited())
	{
		IoSkipCurrentIrpStackLocation(Irp);
		return IoCallDriver(devExt->AttachedToDeviceObject, Irp);
	}

	cfListLock();
    file_sec = cfIsFileCrypting(irpSp->FileObject);
    cfListUnlock();


	//������ɻص�����  �����֪���Ժ�᲻��ʹ�õ�   ����������
//    KeInitializeEvent(&waitEvent, NotificationEvent, FALSE);
//     IoCopyCurrentIrpStackLocationToNext(Irp);
//     IoSetCompletionRoutine(	
// 		Irp,
// 		SfSetInformationCompletion,		//CompletionRoutine
// 		&waitEvent,					//context parameter
// 		TRUE,
// 		TRUE,
// 		TRUE
// 		);	
// 	status = IoCallDriver(devExt->AttachedToDeviceObject, Irp);
// 	if (STATUS_PENDING == status)
// 	{
// 		//�ȴ����
//         status = KeWaitForSingleObject(&waitEvent,
// 			Executive,
// 			KernelMode,
// 			FALSE,
// 			NULL
// 			);
//         ASSERT(STATUS_SUCCESS == status);
// 	}
//	KdPrint(("file object in IRP_MJ_SET_INFORMATION is %08x\n",irpSp->FileObject));

	/////////////////////////////////////͸������//////////////////////////////////
	if (file_sec&&(irpSp->Parameters.SetFile.FileInformationClass == FileAllocationInformation ||
		irpSp->Parameters.SetFile.FileInformationClass == FileEndOfFileInformation ||
		irpSp->Parameters.SetFile.FileInformationClass == FileValidDataLengthInformation ||
		irpSp->Parameters.SetFile.FileInformationClass == FileStandardInformation ||
		irpSp->Parameters.SetFile.FileInformationClass == FileAllInformation ||
		irpSp->Parameters.SetFile.FileInformationClass == FilePositionInformation))
	{
		cfIrpSetInforPre(Irp,irpSp);         //���ô�С
	}
	///////////////////////////////////////////////////////////////////////////////

	if((NoDelNum!=0)&&SfGetFullPath(irpSp->FileObject,fullpath))	
	if(SfGetFullPath(irpSp->FileObject, fullpath))
	{	
        if (SfCompareFullPath(gNoDelete,&NoDelMutex,fullpath))
        {
			KdPrint(("NO!  you can't delete!"));
			Irp->IoStatus.Status = STATUS_ACCESS_DENIED;            
			Irp->IoStatus.Information = 0;    
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			return STATUS_ACCESS_DENIED;
        }
	}

	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(devExt->AttachedToDeviceObject, Irp);

}

NTSTATUS
SfSetInformationCompletion(
							 IN PDEVICE_OBJECT DeviceObject,
							 IN PIRP Irp,
							 IN PVOID Context
							 )
{
	UNREFERENCED_PARAMETER( Irp );
	UNREFERENCED_PARAMETER( DeviceObject );
	KeSetEvent((PKEVENT)Context, IO_NO_INCREMENT, FALSE);
	return STATUS_MORE_PROCESSING_REQUIRED;	//ע�����뷵�����ֵ
}



///////////////////////////////////////////////////////////////////////////////
/////IRP_MJ_DEVICE_CONTROL    IRP��������//////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
NTSTATUS
SfDeviceIOControl(
				  IN PDEVICE_OBJECT DeviceObject,
				  IN PIRP Irp
				  )
{
	NTSTATUS status = STATUS_SUCCESS;
	PIO_STACK_LOCATION irpStack;
	ULONG InputBufferLength;
	ULONG OutputBufferLength;
	ULONG code;
	HANDLE hEvent = NULL;				
	//��������ʽIOCTL
	PVOID* InputBuffer = Irp->AssociatedIrp.SystemBuffer;				
	//�������������
	PVOID* OutputBuffer = Irp->AssociatedIrp.SystemBuffer;
	if (DeviceObject == gSFilterControlDeviceObject) //ֻ��CDO����DeviceIOControl����
	{
		//����
		Irp->IoStatus.Information = 0;
		//�õ���ǰ��ջ
		irpStack = IoGetCurrentIrpStackLocation( Irp );
		//�õ����뻺������С
		InputBufferLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
		//�õ������������С
		OutputBufferLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
		//�õ�IOCTL��
		code=irpStack->Parameters.DeviceIoControl.IoControlCode;								
		
		switch (code)
		{	
			// 		case IOCTL_CONTROLLOG:
			// 			{
			// 				gLogOn=!gLogOn;	
			// 				break;
			// 			}
			// 		case IOCTL_GETLOGBUF:
			// 			{
			// 				PLOG_BUF  oldLog;    
			// 				BOOLEAN             logMutexReleased;
			// 				KdPrint (("IOCTL_GETLOGBUF\n"));
			// 				// If the output buffer is too large to fit into the caller's buffer
			// 				if( LOGBUFSIZE > OutputBufferLength )  {
			// 					//					IoStatus->Status = STATUS_BUFFER_TOO_SMALL;
			// 					return STATUS_BUFFER_TOO_SMALL;
			// 				}
			// 				ExAcquireFastMutex( &LogMutex );
			// 				if( CurrentLog->Len  ||  CurrentLog->Next ) {
			// 					// Start output to a new output buffer
			// 					SfAllocateLog();
			// 					// Fetch the oldest to give to user
			// 					oldLog = SfGetOldestLog();
			// 					if( oldLog != CurrentLog ) {
			// 						logMutexReleased = TRUE;
			// 						ExReleaseFastMutex( &LogMutex );
			// 					} else {
			// 						logMutexReleased = FALSE;
			// 					}
			// 					// Copy it to the caller's buffer
			// 					memcpy( OutputBuffer, oldLog->Data, oldLog->Len );
			// 					// Return length of copied info
			// 					Irp->IoStatus.Information=oldLog->Len;
			// 					// Deallocate buffer - unless its the last one
			// 					if( logMutexReleased ) {
			// 						ExFreePool( oldLog );
			// 					} else {
			// 						CurrentLog->Len = 0;
			// 						ExReleaseFastMutex( &LogMutex );                    
			// 					}
			// 				} else {
			// 					// There is no unread data
			// 					ExReleaseFastMutex( &LogMutex );
			// 					Irp->IoStatus.Information = 0;
			// 				}
			// 			}
			// 			//���logbuf  �����ûͶ��ʹ��
			// 		case IOCTL_ZEROLOGBUF:
			// 			{ 			
			// 				PLOG_BUF  oldLog;
			// 				KdPrint (("IOCTL_ZEROLOGBUF\n"));
			// 				ExAcquireFastMutex( &LogMutex );
			// 				while( CurrentLog->Next )  {
			// 					// Free all but the first output buffer
			// 					oldLog = CurrentLog->Next;
			// 					CurrentLog->Next = oldLog->Next;
			// 					ExFreePool( oldLog );
			// 					NumLog--;
			// 				}
			// 				// Set the output pointer to the start of the output buffer
			// 				CurrentLog->Len = 0;
			// 				//            Sequence = 0;
			// 				ExReleaseFastMutex( &LogMutex );
			// 				break;
			// 			}
			// 			//�µ��������ú� ����Ͳ�����
			// // 		case IOCTL_ADDHIDE_FILE:
			// // 			{	KdPrint(("IOCTL_ADDHIDE_FILE %s",InputBuffer));		
			// // 			AddHideObject(InputBuffer, HIDE_FLAG_FILE);
			// // 			break;
			// // 			}
			// // 		case IOCTL_ADDHIDE_DIRECTORY:
			// // 			{	KdPrint(("IOCTL_ADDHIDE_DIRECTORY %s",InputBuffer));		
			// // 			AddHideObject(InputBuffer, HIDE_FLAG_DIRECTORY);
			// // 			break;
			// // 			}
			
			//��ֹ����
			// ��ʼ����ʱ���������н�ֹ���ʵ��ļ�
			// 		case IOCTL_INIT_NOACCESSFILE  : 
			// 			{
			// 
			// 				break;
			//          }
			// ����������������һ����ֹ���ʵ��ļ�
		case IOCTL_ADD_REPARSE:
			{
				AddReparse((PReparser)InputBuffer);
				break;
			}
		case IOCTL_DEL_REPARSE:
			{
				DelReparse((PWSTR)InputBuffer);
				break;
			}
		case IOCTL_ADD_HIDE:
			{
                AddHideObject((PHider)InputBuffer);
				break;
			}
		case IOCTL_DEL_HIDE:
			{
				DelHideObject((PHider)InputBuffer);
				break;
			}
		case IOCTL_ADD_NOACCESSFILE : 
			{
				SfAddComPathAce((PWSTR)InputBuffer);
				break;
			}
			// ������������ɾ��һ����ֹ���ʵ��ļ�
		case IOCTL_DEL_NOACCESSFILE :
			{
				SfDeleteComPathAce((PWSTR)InputBuffer);
				break;
			}
			
			//��ֹɾ��
			// ��ʼ����ʱ���������н�ֹɾ�����ļ�
			// 		case IOCTL_INIT_NODELETEFILE  : 
			// 			{
			// 
			// 				break;
			//          }
			// ����������������һ����ֹɾ�����ļ�			
		case IOCTL_ADD_NODELETEFILE : 
			{
				SfAddComPathDel((PWSTR)InputBuffer );
				break;
			}
			// ������������ɾ��һ����ֹɾ�����ļ�
		case IOCTL_DEL_NODELETEFILE :
			{
				SfDeleteComPathDel((PWSTR)InputBuffer);
				break;
			}
			//����  ��ûͶ��ʹ��
			// ��ʼ����ʱ�������������ص��ļ� 		
			// 		case IOCTL_ADD_NODELETEFILE : 
			// 			{
			// 				break;
			//          }
			// ����������������һ�����ص��ļ�
			// 		case IOCTL_ADD_NODELETEFILE : 
			// 			{
			// 				break;
			// 			}
			// ������������ɾ��һ�������ص��ļ�
			// 		case IOCTL_DEL_NODELETEFILE :
			// 			{
			// 				break;
			// 			}
		default:
			break;     
		}
		
		Irp->IoStatus.Status = status;
		IoCompleteRequest( Irp, IO_NO_INCREMENT );
		return status;
	}
	IoSkipCurrentIrpStackLocation(Irp);    
	// Call the appropriate file system driver with the request.
	//
	return IoCallDriver(((PSFILTER_DEVICE_EXTENSION)  DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp);	
}

///////////////////////////////////////////////////////////////////////////////
//IRP��������//////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
NTSTATUS
SfPassThrough (//��򵥵�IRP����passthrough 
	       IN PDEVICE_OBJECT DeviceObject,
	       IN PIRP Irp
	       )
{
	ASSERT(!IS_MY_CONTROL_DEVICE_OBJECT( DeviceObject ));
	ASSERT(IS_MY_DEVICE_OBJECT( DeviceObject ));
	IoSkipCurrentIrpStackLocation( Irp );
	return IoCallDriver( ((PSFILTER_DEVICE_EXTENSION) DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp );
}


NTSTATUS
SfCreate (//����create/open���� 
		  IN PDEVICE_OBJECT DeviceObject,
		  IN PIRP Irp
		  )
{
	NTSTATUS status;	
	
	PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT        FileObject = currentIrpStack->FileObject;
	PSFILTER_DEVICE_EXTENSION DevExt = (PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PFILE_OBJECT RelatedFileObject = FileObject->RelatedFileObject;
	//���£����ڻ�ȡ�ļ�·��
	PUNICODE_STRING name;
	GET_NAME_CONTROL nameControl;
	WCHAR FullPathName[MAXPATHLEN]={0};//���ڽ�ֹ����
	
	//�ض���
    PIO_STACK_LOCATION IrpSp;
    PVOID            FileNameBuffer;
//	UNICODE_STRING        NewFileName;
	PUNICODE_STRING FileName = &(currentIrpStack->FileObject->FileName);	
	
	ULONG Return;
	BOOLEAN crypting=FALSE;
	BOOLEAN proc_later=FALSE;
	
	
	//�鿴��ǰ�����Ƿ��Ǽ��ܽ���
	BOOLEAN proc_sec = cfIsCurProcSec();
	PAGED_CODE();	
	
	if (IS_MY_CONTROL_DEVICE_OBJECT(DeviceObject)) 
	{
		Irp->IoStatus.Status = STATUS_SUCCESS;  //�˴��޸�
		Irp->IoStatus.Information = 0;		
		IoCompleteRequest( Irp, IO_NO_INCREMENT );
		return STATUS_SUCCESS;  //�˴��޸�
	}
	
	if (FileObject == NULL)
	{
		IoSkipCurrentIrpStackLocation( Irp );
		return IoCallDriver( ((PSFILTER_DEVICE_EXTENSION) DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp );
	}
	
	ASSERT(IS_MY_DEVICE_OBJECT( DeviceObject ));
	
	if (DevExt->DriveLetter == L'\0') 
	{
		
		UNICODE_STRING DosName;
		
		status = SfVolumeDeviceNameToDosName(&DevExt->DeviceName, &DosName);
		if (NT_SUCCESS(status)) 
		{
			DevExt->DriveLetter = DosName.Buffer[0];
			ExFreePool(DosName.Buffer);	
			
			//ת��Ϊ��д
			if ((DevExt->DriveLetter >= L'a') && (DevExt->DriveLetter <= L'z')) 
			{
				DevExt->DriveLetter += L'A' - L'a';
			}
		} 
		else 
		{
			KdPrint(("sfilter!SfCreate: SfVolumeDeviceNameToDosName(%x) failed(%x)\n",
				DevExt->StorageStackDeviceObject, status));
		}
	}
	
	// 
	// Open Volume Device directly
	//����ֱ�Ӵ򿪴��̵����  ���Ǿ�passthru�� 
	if ((FileObject->FileName.Length == 0) && !RelatedFileObject)
	{
		IoSkipCurrentIrpStackLocation(Irp);
		return IoCallDriver(DevExt->AttachedToDeviceObject, Irp);
	}
	
	//#//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//#//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////			
	/*//�ض���Ĵ��룺				
	if (bRedirectFileOpen)
	{   // 					�����ļ��ض���ʱ,Ӧ����IRP_MJ_CREATE������ض���,����Ӧ��������:
		// 					1,���ͷ�ԭFileObject->FileName;
		// 					2,���·���һ��UNICODE_STRING,������Buffer����Ϊ����򿪵��ļ�*ȫ·��*;
		// 					3,Irp->IoStatus.Status=STATUS_REPARSE;
		// 					Irp->IoStatus.Informiation=IO_REPARSE;
		// 					IoCompleteRequeset(Irp,IO_NO_INCEMENT);
		// 					return STATUS_REPARSE;
		UNICODE_STRING Uceshi;
		RtlInitUnicodeString(&Uceshi, L"\\ceshi.doc" );
		if (RtlEqualUnicodeString(FileName,&Uceshi,TRUE))
		{					
			
			RtlInitUnicodeString(&NewFileName,L"\\??\\C:\\1122\\CFilter.exe");
			FileNameBuffer = ExAllocatePool( NonPagedPool, NewFileName.MaximumLength );
			if (!FileNameBuffer)
			{
				Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				Irp->IoStatus.Information = 0;	
				IoCompleteRequest( Irp, IO_NO_INCREMENT );
				return STATUS_INSUFFICIENT_RESOURCES;
			}
			ExFreePool( FileName->Buffer );
			FileName->Buffer = FileNameBuffer;
			FileName->MaximumLength = NewFileName.MaximumLength;
			RtlCopyUnicodeString( FileName, &NewFileName );
			Irp->IoStatus.Status = STATUS_REPARSE;
			Irp->IoStatus.Information = IO_REPARSE;
			IoCompleteRequest( Irp, IO_NO_INCREMENT );
			return STATUS_REPARSE;
		}	
	}*/
	//#/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//#/////////////////////////////////////////////////////////////////////////////////////////////////////////////
				//����Ǽ��ܽ���
	if ((cfListInited()) && proc_sec)
	{
		Return=cfIrpCreatePre(Irp,currentIrpStack,FileObject,((PSFILTER_DEVICE_EXTENSION) DeviceObject->DeviceExtension)->AttachedToDeviceObject);
		
	}
	else
	{
		//��ߵ�����ͨ������������Ϊ��ͨ���̣��������һ�����ڼ��ܵ��ļ�
		//��Ϊ��߻�û�취�жϣ�������GO_ON
		Return = SF_IRP_GO_ON;
	}
				
	if (Return == SF_IRP_PASS)
	{
		//��֪����ߵ��⼸��irpҪ��ô������Ҫ�ǲ�֪���᲻�ᣬ�ͽ�ֹ���ʳ�ͻ
		//��ʱ�Ȱ���ֱ�Ӳ��ܣ����������·�������ȥִ�н�ֹ���ʵĴ���
		goto SkipHandle;
	}
	if (Return == SF_IRP_COMPLETED)
	{
		return  status;
	}
	if(Return == SF_IRP_GO_ON)
	{
		KEVENT waitEvent; 
		PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
		PFILE_OBJECT        FileObject = currentIrpStack->FileObject;
		
		//////////////////////////////////ԭ������////////////////////////
		//����κεĴ���ڹ�ϣ���е�fileobject/name
		if (FileObject)//ż���Ӹ��жϰ�  ��ֹΪ�� BSOD    
		{SfFreeHashEntry( FileObject );}
		//====================================================================================
		
		//�ȴ��¼����ڵȴ��������
		KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );
		//������ǰI/O��ջ����һ����ջ���������ǵ��������
		IoCopyCurrentIrpStackLocationToNext( Irp );
		IoSetCompletionRoutine(
			Irp,
			SfCreateCompletion,//�������
			&waitEvent,
			TRUE,
			TRUE,
			TRUE );
		//����ջ�е���һ������
		status = IoCallDriver( ((PSFILTER_DEVICE_EXTENSION) DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp );
		//�ȴ��ں��¼�
		if (STATUS_PENDING == status) {
			NTSTATUS localStatus = KeWaitForSingleObject(	&waitEvent, 
				Executive,//�ȴ���ԭ��
				KernelMode,//�������������
				FALSE,
				NULL//���޵ĵ���ȥ
				);
			ASSERT(STATUS_SUCCESS == localStatus);
		}
		
		//��֤IoCompleteRequest��������
		ASSERT(KeReadStateEvent(&waitEvent) ||
			!NT_SUCCESS(Irp->IoStatus.Status));		
		
		
		proc_later = cfIsCurProcSec();    //�ж��Ƿ�Ϊ���ܽ���
		//PIO_STACK_LOCATION irpsp = IoGetCurrentIrpStackLocation(Irp);
		// �Ƿ���һ���Ѿ������ܽ��̴򿪵��ļ�
		cfListLock();
		crypting = cfIsFileCrypting(FileObject);
		cfListUnlock();
		
		if (proc_later)
		{
			//�Ǽ��ܽ��̣������ټ��������У���׷�����������
			
			ASSERT(crypting == FALSE);
			if (!cfFileCryptAppendLk(FileObject))
			{
				//������벻�ɹ����Ƿ���������
				IoCancelFileOpen(((PSFILTER_DEVICE_EXTENSION) DeviceObject->DeviceExtension)->AttachedToDeviceObject,FileObject);
				Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
				Irp->IoStatus.Information = 0;
				//KdPrint(("OnSfilterIrpPost: file %wZ failed to call cfFileCryptAppendLk!!!\r\n",&file->FileName));
			} 
			else
			{
				//KdPrint(("OnSfilterIrpPost: file %wZ begin to crypting.\r\n",&file->FileName));
			}
		} 
		else
		{
			// ����ͨ���̡������Ƿ��Ǽ����ļ�������Ǽ����ļ���
			// ������������
			if(crypting)
			{
				IoCancelFileOpen(((PSFILTER_DEVICE_EXTENSION) DeviceObject->DeviceExtension)->AttachedToDeviceObject,FileObject);
				Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
				Irp->IoStatus.Information = 0;
			}
		}
		
		//�������ڻ�ȡ�ļ�·��
		IrpSp = IoGetCurrentIrpStackLocation( Irp );	
		//����ļ����������
		name = SfGetFileName( IrpSp->FileObject,
			Irp->IoStatus.Status, 
			&nameControl );
		SfAddIntoHashTable(//������Ҫ����log��¼ ������������������
			FileObject,
			name, 
			&DevExt->DeviceName,
			DevExt->DriveLetter
			);	
		SfGetFileNameCleanup( &nameControl );//���
		
		if(SfGetFullPath(FileObject,FullPathName))
		{
			if(NoAceNum!=0)
			{			
				KdPrint(("%ws\n",FullPathName));
				if(SfCompareFullPath(gNoAccess,&NoAceMutex,FullPathName))
				{
					KdPrint(("NO!  you can't access!"));
					Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
					Irp->IoStatus.Information = 0;    
					IoCompleteRequest(Irp, IO_NO_INCREMENT);
					return STATUS_ACCESS_DENIED;	
				}				
			}
			
			//���̷��������ļ�
			if(NoHidAceNum!=0)
			{			
				KdPrint(("%ws\n",FullPathName));
				if(SfCompareFullPath(gHidNoAccess,&NoHidAceMutex,FullPathName))
				{
					KdPrint(("NO!  you can't access!"));
					Irp->IoStatus.Status = STATUS_OBJECT_PATH_SYNTAX_BAD;
					Irp->IoStatus.Information = 0;    
					IoCompleteRequest(Irp, IO_NO_INCREMENT);
					return STATUS_OBJECT_PATH_SYNTAX_BAD;
				}
			}
			

			if (ReparseNum!=0)
			{   
				if (FindReparsePath(FullPathName)==TRUE)
				{					
					FileNameBuffer = ExAllocatePool( NonPagedPool,sizeof(WCHAR)*1024);
					if (!FileNameBuffer)
					{
						Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
						Irp->IoStatus.Information = 0;	
						IoCompleteRequest( Irp, IO_NO_INCREMENT );
						return STATUS_INSUFFICIENT_RESOURCES;
					}
					ExFreePool( FileName->Buffer );

			        RtlZeroMemory(FileNameBuffer,2*MAXPATHLEN);
					RtlCopyMemory(FileNameBuffer,FullPathName,2*wcslen(FullPathName));					
					//����filename
					FileName->Length=2*wcslen(FullPathName);				
					FileName->Buffer = FileNameBuffer;
					FileName->MaximumLength = 2*MAXPATHLEN;
					
					Irp->IoStatus.Status = STATUS_REPARSE;
					Irp->IoStatus.Information = IO_REPARSE;	
					IoCompleteRequest( Irp, IO_NO_INCREMENT );
					return STATUS_REPARSE;
				}
			}	
		}
		
		//  ����status���Ҽ�������IRP
		status = Irp->IoStatus.Status;
		IoCompleteRequest( Irp, IO_NO_INCREMENT );
		return status;
	}
SkipHandle:
	IoSkipCurrentIrpStackLocation( Irp );
	return IoCallDriver( ((PSFILTER_DEVICE_EXTENSION) DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp );
}


NTSTATUS
SfCreateCompletion (
		    IN PDEVICE_OBJECT DeviceObject,
		    IN PIRP Irp,
		    IN PVOID Context
		    )
		    //������� �������Ե��� ��ô����Ĺ��ܾ��Ǵ�ӡ�ɹ����ļ�ϵͳ�򿪵��ļ����ļ���
{
	PKEVENT event = Context;
	UNREFERENCED_PARAMETER( DeviceObject );
	UNREFERENCED_PARAMETER( Irp );
	ASSERT(IS_MY_DEVICE_OBJECT( DeviceObject ));
	KeSetEvent(event, IO_NO_INCREMENT, FALSE);//���õȴ��¼�
	return STATUS_MORE_PROCESSING_REQUIRED;//������Ҫ��һ������
}

NTSTATUS
SfCleanup (
	   IN PDEVICE_OBJECT DeviceObject,
	   IN PIRP Irp
	   )
	   //cleanup request ����
{

	PAGED_CODE();
	if (DeviceObject == gSFilterControlDeviceObject) 
	{
		Irp->IoStatus.Status = STATUS_SUCCESS;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest( Irp, IO_NO_INCREMENT );
		return STATUS_SUCCESS;
	}
	

	
	ASSERT(!IS_MY_CONTROL_DEVICE_OBJECT( DeviceObject ));
	ASSERT(IS_MY_DEVICE_OBJECT( DeviceObject ));
	IoSkipCurrentIrpStackLocation( Irp );
	return IoCallDriver( ((PSFILTER_DEVICE_EXTENSION) DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp );
}

NTSTATUS
SfClose (
	 IN PDEVICE_OBJECT DeviceObject,
	 IN PIRP Irp
	 )
	 //cleanup/close request ����
{
	NTSTATUS status;
	PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT        FileObject = currentIrpStack->FileObject;
	BOOLEAN file_sec;
	PAGED_CODE();
	//  Sfilter���������CDO������ ����û��CDO��IRPͨ��  filespy�ĸ�����Ӧ�ò�ͬ 
	/////////////////////////////////////////////////////////////////////////
	//�˴�Ϊ��Ӵ���Σ�һ��Ҫ�ڴ�λ����ӣ���ӵ�������ӻ�����
	if (DeviceObject == gSFilterControlDeviceObject) {
		Irp->IoStatus.Status = STATUS_SUCCESS;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest( Irp, IO_NO_INCREMENT );
		return STATUS_SUCCESS;
	}


	ASSERT(!IS_MY_CONTROL_DEVICE_OBJECT( DeviceObject ));
	ASSERT(IS_MY_DEVICE_OBJECT( DeviceObject ));     

	if (FileObject)
	{
		SfFreeHashEntry( FileObject );
	}

	//////////////////////////͸������////////////////////////////////////////////
	if (!cfListInited())
	{
		IoSkipCurrentIrpStackLocation( Irp );
		return IoCallDriver( ((PSFILTER_DEVICE_EXTENSION) DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp );
	}
	cfListLock();
    file_sec = cfIsFileCrypting(FileObject);
    cfListUnlock();
	//������Ƽ��������еĶ���
	if (file_sec)
	{
		KEVENT waitEvent; 
		BOOLEAN crypting;
		
		//�ȴ��¼����ڵȴ��������
		KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );
		//������ǰI/O��ջ����һ����ջ���������ǵ��������
		IoCopyCurrentIrpStackLocationToNext( Irp );
		IoSetCompletionRoutine(
			Irp,
			SfCreateCompletion,//�������
			&waitEvent,
			TRUE,
			TRUE,
			TRUE );
		//����ջ�е���һ������
		status = IoCallDriver( ((PSFILTER_DEVICE_EXTENSION) DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp );
		//�ȴ��ں��¼�
		if (STATUS_PENDING == status) {
			NTSTATUS localStatus = KeWaitForSingleObject(	&waitEvent, 
				Executive,//�ȴ���ԭ��
				KernelMode,//�������������
				FALSE,
				NULL//���޵ĵ���ȥ
				);
			ASSERT(STATUS_SUCCESS == localStatus);
		}
		//��֤IoCompleteRequest��������
		ASSERT(KeReadStateEvent(&waitEvent) ||
			!NT_SUCCESS(Irp->IoStatus.Status));		
		
		{
			PIO_STACK_LOCATION  irpSpLater = IoGetCurrentIrpStackLocation(Irp);
			PFILE_OBJECT        file = irpSpLater->FileObject;
			
			cfListLock();
			crypting = cfIsFileCrypting(file);
            cfListUnlock();
			
			ASSERT(crypting);
            cfCryptFileCleanupComplete(file);   //ɾ��һ���ڵ�
		}		
		
		//  ����status���Ҽ�������IRP
		status = Irp->IoStatus.Status;
		IoCompleteRequest( Irp, IO_NO_INCREMENT );
		
		return status;
	}
	//////////////////////////͸������////////////////////////////////////////////

	IoSkipCurrentIrpStackLocation( Irp );
	return IoCallDriver( ((PSFILTER_DEVICE_EXTENSION) DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp );
}


NTSTATUS
SfFsControl (
	     IN PDEVICE_OBJECT DeviceObject,
	     IN PIRP Irp
	     )
	     //�����̱�������IRP_MJ_FILE_SYSTEM_CONTROL Ҳ�Ǽ򵥵�passed through ������Щ��Ҫ���⴦��
{
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation( Irp );
	PAGED_CODE();
	
	ASSERT(!IS_MY_CONTROL_DEVICE_OBJECT( DeviceObject ));
	ASSERT(IS_MY_DEVICE_OBJECT( DeviceObject ));
	switch (irpSp->MinorFunction) 
	{
        
	case IRP_MN_MOUNT_VOLUME:
		return SfFsControlMountVolume( DeviceObject, Irp );
        
	case IRP_MN_LOAD_FILE_SYSTEM://��һ���ļ�ʶ�����������ģ����������������ļ�ϵͳ��ʱ�򣬻����һ��������irp
		//		��������Ѿ������ļ�ϵͳʶ���������ھ�Ӧ�ý���󶨲������豸��ͬʱ�����µ��豸ȥ������ļ�ϵͳ
		return SfFsControlLoadFileSystem( DeviceObject, Irp );
		
	}    
    
	// �������е��������ļ�ϵͳ�������󴩹�
	IoSkipCurrentIrpStackLocation( Irp );
	
	return IoCallDriver( ((PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp );
}

NTSTATUS
SfFsControlCompletion (
		       IN PDEVICE_OBJECT DeviceObject,
		       IN PIRP Irp,
		       IN PVOID Context
		       )
		       //������� Control�� ���ֻ��ֱ�ӵİ����̷��� û��ʲô����
{
	UNREFERENCED_PARAMETER( DeviceObject );//UNREFERENCED_PARAMETER ���������ڣ�ȥ��C ����������û��ʹ�õ����������������һ�����档
	UNREFERENCED_PARAMETER( Irp );//UNREFERENCED_PARAMETER�����ã�����
	ASSERT(IS_MY_DEVICE_OBJECT( DeviceObject ));
	ASSERT(Context != NULL);
	if (IS_WINDOWSXP_OR_LATER()) {
		
		KeSetEvent((PKEVENT)Context, IO_NO_INCREMENT, FALSE);
		
	} 

	return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
SfFsControlMountVolume (
			IN PDEVICE_OBJECT DeviceObject,
			IN PIRP Irp
			)
{
	PSFILTER_DEVICE_EXTENSION devExt = DeviceObject->DeviceExtension;
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation( Irp );
	PDEVICE_OBJECT newDeviceObject;
	PDEVICE_OBJECT storageStackDeviceObject;
	PSFILTER_DEVICE_EXTENSION newDevExt;
	NTSTATUS status;
	BOOLEAN isShadowCopyVolume;
	KEVENT waitEvent;


	PAGED_CODE();
	ASSERT(IS_MY_DEVICE_OBJECT( DeviceObject ));
	ASSERT(IS_DESIRED_DEVICE_TYPE(DeviceObject->DeviceType));
	storageStackDeviceObject = irpSp->Parameters.MountVolume.Vpb->RealDevice;//�ȱ���  ��ֹ�����Ĵ�����޸����ֵ
	//VBP�Ѽʴ洢ý���豸������ļ�ϵͳ�ϵľ��豸������ϵ����.  ��VPB ���ٵõ���Ӧ�ľ��豸
	
	status = SfIsShadowCopyVolume ( storageStackDeviceObject,//�ж��Ƿ�Ϊ��Ӱ 
		&isShadowCopyVolume );
	
	if (NT_SUCCESS(status) && 
		isShadowCopyVolume ) 
	{
		//����һ������  ��Ӱ�Ͳ�����
		IoSkipCurrentIrpStackLocation( Irp );
		return IoCallDriver( devExt->AttachedToDeviceObject, Irp );
	}

	status = IoCreateDevice( gSFilterDriverObject,
		sizeof( SFILTER_DEVICE_EXTENSION ),
		NULL,
		DeviceObject->DeviceType,//�豸���� ����������󶨵�������������������ǵĹ����������Ŀ����豸������ͬ
		0, 
		FALSE,
		&newDeviceObject );//�����µ��豸����ö���󶨵��ļ�ϵͳ�ľ��豸������
	if (!NT_SUCCESS( status )) 
	{//����ʧ��
		// ������ܰ� ˵��������� 
		KdPrint(( "SFilter!SfFsControlMountVolume: Error creating volume device object, status=%08x\n", status ));
		Irp->IoStatus.Information = 0;
		Irp->IoStatus.Status = status;
		IoCompleteRequest( Irp, IO_NO_INCREMENT );//���� IRP ����ʧ��
		return status;
	}

	//  ��д�豸��չ
	newDevExt = newDeviceObject->DeviceExtension;//����µ��豸���豸��չ
	
	//��дǰ�ȱ�֤�����ǿյ�
	RtlZeroMemory(newDevExt, sizeof(SFILTER_DEVICE_EXTENSION));
	
	newDevExt->StorageStackDeviceObject = storageStackDeviceObject;//ǰ�汣��Ĵ����µ��豸��չ��
	RtlInitEmptyUnicodeString( &newDevExt->DeviceName, 
		newDevExt->DeviceNameBuffer, 
		sizeof(newDevExt->DeviceNameBuffer));
	SfGetObjectName( storageStackDeviceObject,  //����豸��
		&newDevExt->DeviceName );
	
	ASSERT(IS_WINDOWSXP_OR_LATER()); //����Ǳ�Ȼ��	
	KeInitializeEvent( 
					  &waitEvent, 
					  NotificationEvent, 
					  FALSE);
	
	IoCopyCurrentIrpStackLocationToNext ( Irp );
	IoSetCompletionRoutine( Irp,
		SfFsControlCompletion,//�������
		&waitEvent,     //context parameter
		TRUE,
		TRUE,
		TRUE );
	status = IoCallDriver( devExt->AttachedToDeviceObject, Irp );
	//�ȴ����
	if (STATUS_PENDING == status) {
		status = KeWaitForSingleObject( &waitEvent,
			Executive,
			KernelMode,
			FALSE,
			NULL );
		ASSERT( STATUS_SUCCESS == status );
	}
	ASSERT(KeReadStateEvent(&waitEvent) ||
		!NT_SUCCESS(Irp->IoStatus.Status));
	status = SfFsControlMountVolumeComplete( 
											DeviceObject,
											Irp,
											newDeviceObject );
	
	return status;
}

NTSTATUS
SfFsControlMountVolumeComplete (
				IN PDEVICE_OBJECT DeviceObject,
				IN PIRP Irp,
				IN PDEVICE_OBJECT NewDeviceObject
				)
				//post-Mount work������PASSIVE_LEVEL������
{
	PVPB vpb;
	PSFILTER_DEVICE_EXTENSION newDevExt;
	PIO_STACK_LOCATION irpSp;
	PDEVICE_OBJECT attachedDeviceObject;
	NTSTATUS status;
	PAGED_CODE();
	UNREFERENCED_PARAMETER(DeviceObject);
	newDevExt = NewDeviceObject->DeviceExtension;//����豸��չ
	irpSp = IoGetCurrentIrpStackLocation( Irp );//��ñ����豸��Ӧ��IO_STACK_LOCATION
	//����ʵ���豸�����л�õ�ǰ��VPB���浽���ǵ��豸��չ�� 
	//����������ΪVPB in the IRP stackҲ�������ǵ�ǰ�õ���VPB 
	//������ļ�ϵͳҲ���ı�VPBs��������Ƿ����װʱ��
	
	//���⣺StorageStackDeviceObject��ֵ����
	//��newDevExt->StorageStackDeviceObject(��PDEVICE_OBJECT����) = storageStackDeviceObject(��PDEVICE_OBJECT����);//ǰ�汣��Ĵ����µ��豸��չ�С�
	//��ֵ
	//�������storageStackDeviceObject = irpSp->Parameters.MountVolume.Vpb->RealDevice;//�ȱ���  ��ֹ�����Ĵ�����޸����ֵ
	//irpSp->Parameters.MountVolume.Vpb->RealDevice��PDEVICE_OBJECT����
	//����	irpSp->Parameters.MountVolume.Vpb->RealDevice==newDevExt->StorageStackDeviceObject->Vpb��
	//��ôVBP������ݽṹ��ֻ��һ������RealDevice���� �����������PDEVICE_OBJECT���ͣ�
	//��ô������PVPB==PDEVICE_OBJECT��  ������Щ��ֵ��������ǿ�Ƶ�����ת��
	vpb = newDevExt->StorageStackDeviceObject->Vpb;//����ǰ�汣�����vpb,���
	if (vpb != irpSp->Parameters.MountVolume.Vpb) {//���ǿ���VPB�Ƿ�ı���  �ı��� �ʹ�ӡ����Ϣ
		KdPrint(("SFilter!SfFsControlMountVolume:              VPB in IRP stack changed   %p IRPVPB=%p VPB=%p\n",
			vpb->DeviceObject,
			irpSp->Parameters.MountVolume.Vpb,
			vpb));
	}

	//���Ƿ�װ�ɹ�
	if (NT_SUCCESS( Irp->IoStatus.Status ))
	{
		// ���һ��������,�Ա����ǿ���ԭ�ӵ��ж������Ƿ�󶨹�һ�����豸.����Է�ֹ
		// ���Ƕ�һ��������Ρ�.
		ExAcquireFastMutex( &gSfilterAttachLock );
		if (!SfIsAttachedToDevice( vpb->DeviceObject, &attachedDeviceObject )) 
		{//û�а󶨵��豸��ȥ
			status = SfAttachToMountedDevice( vpb->DeviceObject,// ���� ����������İ�.
				NewDeviceObject );
			if (!NT_SUCCESS( status )) 
			{ //��ʧ�� ��ôֻ�����
				SfCleanupMountedDevice( NewDeviceObject );
				IoDeleteDevice( NewDeviceObject );
			}
			ASSERT( NULL == attachedDeviceObject );
		} 
		else 
		{//����ȥ�ľ���ʾ��Ϣ
			SfCleanupMountedDevice( NewDeviceObject );
			IoDeleteDevice( NewDeviceObject );
			ObDereferenceObject( attachedDeviceObject );//���ټ���
		}
		ExReleaseFastMutex( &gSfilterAttachLock );//�ͷ���
	} 
	else 
	{//��װ����ʧ�ܵĴ���

		//�����ɾ�����Ǵ������豸����
		SfCleanupMountedDevice( NewDeviceObject );
		IoDeleteDevice( NewDeviceObject );
	}
	//�������� ���Ǳ���Ҫ�ڽ���ǰ����status ��Ϊ�ڽ���IRP�����ǽ��޷��ٻ�ã���Ҳ��ᱻ�ͷţ�
	status = Irp->IoStatus.Status;
	IoCompleteRequest( Irp, IO_NO_INCREMENT );
	return status;
}
NTSTATUS
SfFsControlLoadFileSystem (
						   IN PDEVICE_OBJECT DeviceObject,
						   IN PIRP Irp
						   )
{
	PSFILTER_DEVICE_EXTENSION devExt = DeviceObject->DeviceExtension;
	NTSTATUS status;
	KEVENT waitEvent; 
	PAGED_CODE();
	
	ASSERT(IS_WINDOWSXP_OR_LATER()); 
		
	KeInitializeEvent( &waitEvent, 
		NotificationEvent, 
		FALSE );
	IoCopyCurrentIrpStackLocationToNext( Irp );        
	IoSetCompletionRoutine( Irp,
		SfFsControlCompletion,
		&waitEvent,     //context parameter
		TRUE,
		TRUE,
		TRUE );
	status = IoCallDriver( devExt->AttachedToDeviceObject, Irp );
	//  �ȴ��������
	if (STATUS_PENDING == status) {
		status = KeWaitForSingleObject( &waitEvent,
			Executive,
			KernelMode,
			FALSE,
			NULL );
		ASSERT( STATUS_SUCCESS == status );
	}

	ASSERT(KeReadStateEvent(&waitEvent) ||
		!NT_SUCCESS(Irp->IoStatus.Status));
	status = SfFsControlLoadFileSystemComplete( DeviceObject,
		Irp );
	
	return status;
}


NTSTATUS
SfFsControlLoadFileSystemComplete (
				   IN PDEVICE_OBJECT DeviceObject,
				   IN PIRP Irp
				   )
{
	PSFILTER_DEVICE_EXTENSION devExt;
	NTSTATUS status;
	PAGED_CODE();
	devExt = DeviceObject->DeviceExtension;

	//  �������� status 
	if (!NT_SUCCESS( Irp->IoStatus.Status ) && 
		(Irp->IoStatus.Status != STATUS_IMAGE_ALREADY_LOADED)) {
		SfAttachDeviceToDeviceStack( DeviceObject, 
			devExt->AttachedToDeviceObject,
			&devExt->AttachedToDeviceObject );
		ASSERT(devExt->AttachedToDeviceObject != NULL);
	} else {
		//���سɹ� ����豸��ɾ���豸����  
		SfCleanupMountedDevice( DeviceObject );
		IoDeleteDevice( DeviceObject );
	}
	//�����������
	status = Irp->IoStatus.Status;
	IoCompleteRequest( Irp, IO_NO_INCREMENT );
	return status;
}

NTSTATUS
SfAttachDeviceToDeviceStack (
			     IN PDEVICE_OBJECT SourceDevice,
			     IN PDEVICE_OBJECT TargetDevice,
			     IN OUT PDEVICE_OBJECT *AttachedToDeviceObject
			     )
{
	PAGED_CODE();
	
	ASSERT (IS_WINDOWSXP_OR_LATER()); 
	
		ASSERT( NULL != gSfDynamicFunctions.AttachDeviceToDeviceStackSafe );
		
		return (gSfDynamicFunctions.AttachDeviceToDeviceStackSafe)( SourceDevice,
			TargetDevice,
			AttachedToDeviceObject );
	
} 
NTSTATUS
SfAttachToFileSystemDevice(
			   IN PDEVICE_OBJECT DeviceObject,
			   IN PUNICODE_STRING DeviceName
			   )
{
	PDEVICE_OBJECT newDeviceObject; // ���Ǵ����Ĺ����豸����
	PSFILTER_DEVICE_EXTENSION devExt; // �豸������չ
	NTSTATUS status;
	UNICODE_STRING fsName;// �ļ�������
	WCHAR fsNameBuffer[MAX_DEVNAME_LENGTH];
	UNICODE_STRING fsrecName; // �ļ�ϵͳʶ������  
	PAGED_CODE();
	
	//
	// ��������ļ�ϵͳ����,�򷵻�
	if(!IS_DESIRED_DEVICE_TYPE(DeviceObject->DeviceType))
	{
		
		return STATUS_SUCCESS;
	}

	//�ų��ļ�ϵͳʶ����	
	RtlInitEmptyUnicodeString(&fsName,// ������ʼ���ļ�������
		fsNameBuffer,
		sizeof(fsNameBuffer));
	

		// �������ǲ�Ҫ��ʶ����
		SfGetObjectName(DeviceObject->DriverObject,// ���ָ���ļ�ϵͳ��������
			&fsName);
		// ��ʼ���ļ�ϵͳʶ������
		// ע��,������΢���׼���ļ�ʶ������
		// ���в���׼��,�ڿ��Ʋ����й���
		RtlInitUnicodeString(&fsrecName, L"\\FileSystem\\Fs_Rec");
		if (RtlCompareUnicodeString(&fsName, &fsrecName, TRUE) == 0)// ���˵�΢���׼�ļ�ʶ����
		{
			
			return STATUS_SUCCESS;
		}
	
	status = IoCreateDevice(gSFilterDriverObject,
		sizeof(SFILTER_DEVICE_EXTENSION),
		NULL,
		DeviceObject->DeviceType,
		0,
		FALSE,
		&newDeviceObject);
	if (!NT_SUCCESS(status))
	{
		return status;
	}
	//
	// �����Ǹ����ļ�ϵͳ�����豸����ı�־�������µ��豸������
	// ��Ϊ�����µ��豸����Ҫ���ӵ��ļ�ϵͳ���豸ջ��,����,���ǵ�
	// �豸���������ļ�ϵͳ���豸����ı�־һ��.
	// ��Ҳ���豸ջ�ƶ��Ĺ����
	if ( FlagOn( DeviceObject->Flags, DO_BUFFERED_IO ))
	{SetFlag( newDeviceObject->Flags, DO_BUFFERED_IO );}
	if ( FlagOn( DeviceObject->Flags, DO_DIRECT_IO ))
	{SetFlag( newDeviceObject->Flags, DO_DIRECT_IO );}
	if ( FlagOn( DeviceObject->Characteristics, FILE_DEVICE_SECURE_OPEN ) )
	{SetFlag( newDeviceObject->Characteristics, FILE_DEVICE_SECURE_OPEN );}
	// �豸��չ
	devExt = newDeviceObject->DeviceExtension;

	RtlZeroMemory(devExt, sizeof(SFILTER_DEVICE_EXTENSION));
	// �ﶨ���ǵ��豸�����ļ�ϵͳ���豸���ƶ�����豸ջ��
	// ע��: ԭ���豸ջ���˵��豸����,���Ӻ�Ϊ�����豸�������һ���豸����
	// �����������豸������豸��չ��.
	status = SfAttachDeviceToDeviceStack(newDeviceObject,
		DeviceObject,
		&devExt->AttachedToDeviceObject);
	if(!NT_SUCCESS(status))
	{
		goto ErrorCleanupDevice;
	}
	// ������ļ�ϵͳ�����豸����
	//���������sfilter��û�У�����������Ч�Ĵ��� ����������
	RtlInitEmptyUnicodeString(&devExt->DeviceName,
		devExt->DeviceNameBuffer,
		sizeof(devExt->DeviceNameBuffer));
	RtlCopyUnicodeString(&devExt->DeviceName, DeviceName);


	ASSERT (IS_WINDOWSXP_OR_LATER());
	
		ASSERT(NULL != gSfDynamicFunctions.EnumerateDeviceObjectList &&
			NULL != gSfDynamicFunctions.GetDiskDeviceObject &&
			NULL != gSfDynamicFunctions.GetDeviceAttachmentBaseRef &&
			NULL != gSfDynamicFunctions.GetLowerDeviceObject
			);
		//ö�����еĵ�ǰ��װ���豸 ��������
		status = SfEnumerateFileSystemVolumes(DeviceObject, &fsName);
		if (!NT_SUCCESS(status))
		{
			IoDetachDevice(devExt->AttachedToDeviceObject);
			goto ErrorCleanupDevice;
		}
	
	return STATUS_SUCCESS;
ErrorCleanupDevice:// ����:
	SfCleanupMountedDevice( newDeviceObject );
	IoDeleteDevice( newDeviceObject );
	return status;
}
VOID
SfDetachFromFileSystemDevice(
			     IN PDEVICE_OBJECT DeviceObject//Ҫ������ļ�ϵͳ�豸
			     )
			     //������������²㣿�����ļ�ϵͳ�豸�����⽫ɨ�踽����Ѱ�����ǰ󶨵��豸���� ����ҵ��ʹ����з���
{
	PDEVICE_OBJECT ourAttachedDevice;
//	PSFILTER_DEVICE_EXTENSION devExt;
	PAGED_CODE();
	// �����������ļ�ϵͳ�豸 ���ǲ�������Ҫ��
	ourAttachedDevice = DeviceObject->AttachedDevice;
	while (NULL != ourAttachedDevice) 
	{		//ѭ���ҵ�������
		if (IS_MY_DEVICE_OBJECT(ourAttachedDevice)) 
		{	//�����ǵ��豸		
			
			SfCleanupMountedDevice(ourAttachedDevice);
			IoDetachDevice(DeviceObject);//�ͷŸ��豸
			IoDeleteDevice(ourAttachedDevice);//���֮
			return;// �����ú�����  ��ɾ��һ���豸
		}

		// �������ǵ��豸 ���Ծͼ���������
		DeviceObject = ourAttachedDevice;
		ourAttachedDevice = ourAttachedDevice->AttachedDevice;
	}
}

NTSTATUS
SfEnumerateFileSystemVolumes (
							  IN PDEVICE_OBJECT FSDeviceObject,//����Ҫ�оٵ��ļ�ϵͳ�豸����
							  IN PUNICODE_STRING Name//�Ѷ����UNICODE�ַ������ڼ������� ���������Ϊ�˼���ջ�ϵ��ַ����ĸ���
							  ) 
							  //�о����еľ��豸����ǰ���ļ�ϵͳ�еģ���������
							  //����������Ϊ�������������κ�ʱ����� ��ʱҲ���Ѿ��о���ϵͳ�а�װ��
{
	PDEVICE_OBJECT newDeviceObject;
	PSFILTER_DEVICE_EXTENSION newDevExt;
	PDEVICE_OBJECT *devList;
	PDEVICE_OBJECT storageStackDeviceObject;
	NTSTATUS status;
	ULONG numDevices;
	ULONG i;
	BOOLEAN isShadowCopyVolume;		
	PAGED_CODE();
	status = (gSfDynamicFunctions.EnumerateDeviceObjectList)(//�⺯��Ҫ���ε��� ��һ��Ϊ��Ѱ��Ҫ���Ŀռ�
		FSDeviceObject->DriverObject,
		NULL,
		0,
		&numDevices);
	if (NT_SUCCESS( status )) 
	{
		return status;
	}
	ASSERT(STATUS_BUFFER_TOO_SMALL == status);		// Ϊ��֪��С���豸�������ڴ� 
	numDevices += 8;        // ��΢��һЩ ����ֹ�����������!��
	devList = ExAllocatePoolWithTag( 
									NonPagedPool, 
									(numDevices * sizeof(PDEVICE_OBJECT)), 
									SFLT_POOL_TAG );
	if (NULL == devList) 
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	// ����豸�������ִ����ֻ��over��
	ASSERT( NULL != gSfDynamicFunctions.EnumerateDeviceObjectList );
	status = (gSfDynamicFunctions.EnumerateDeviceObjectList)(//�ڶ��ε��� �õ��豸�ı�����
															 FSDeviceObject->DriverObject,
															 devList,
															 (numDevices * sizeof(PDEVICE_OBJECT)),
															 &numDevices);
	if (!NT_SUCCESS( status ))  
	{
		ExFreePoolWithTag( devList, SFLT_POOL_TAG );
		return status;
	}
	// ���� ������Ҫ����Щ        
	for (i=0; i < numDevices; i++) 
	{
		storageStackDeviceObject = NULL;
		try 
		{
			//CDO �����ϵ����� �Ѿ������	�����ֲ��ð���
			if ((devList[i] == FSDeviceObject) ||
				(devList[i]->DeviceType != FSDeviceObject->DeviceType) ||
				SfIsAttachedToDevice( devList[i], NULL )) 
			{
				leave;
			}
			// ������豸������û�� �еĻ���˵�������г���һ����CDO����FastFat��
			SfGetBaseDeviceObjectName( devList[i], Name );
			if (Name->Length > 0) 
			{
				leave;
			}
			// ��������ģ����̣��洢ջ���豸�������ļ�ϵͳ�豸�����й� �����һ�������豸����ֻ����ȥ�� 
			ASSERT( NULL != gSfDynamicFunctions.GetDiskDeviceObject );
			status = (gSfDynamicFunctions.GetDiskDeviceObject)( devList[i], &storageStackDeviceObject );
			if (!NT_SUCCESS( status )) 
			{
				leave;
			}

			//�ж��ǲ���һ����Ӱ �ǵĻ� �Ͳ��� 
			status = SfIsShadowCopyVolume (storageStackDeviceObject, &isShadowCopyVolume );
			if (NT_SUCCESS(status) && isShadowCopyVolume ) 
			{
				UNICODE_STRING shadowDeviceName;
				WCHAR shadowNameBuffer[MAX_DEVNAME_LENGTH];
				// ���debug�������ڴ�ӡ 
				RtlInitEmptyUnicodeString( &shadowDeviceName, 
					shadowNameBuffer, 
					sizeof(shadowNameBuffer) );
				SfGetObjectName( storageStackDeviceObject, 
					&shadowDeviceName );
				leave;
			}
			// ����һ���µ��豸�������� 
			status = IoCreateDevice( gSFilterDriverObject,
				sizeof( SFILTER_DEVICE_EXTENSION ),
				NULL,
				devList[i]->DeviceType,
				0,
				FALSE,
				&newDeviceObject );
			if (!NT_SUCCESS( status )) 
			{
				leave;
			}
			// ���ô����豸����	
			newDevExt = newDeviceObject->DeviceExtension;
			RtlZeroMemory(newDevExt, sizeof(SFILTER_DEVICE_EXTENSION));
			newDevExt->StorageStackDeviceObject = storageStackDeviceObject;
			// ���ô洢ջ�豸����
			RtlInitEmptyUnicodeString( &newDevExt->DeviceName,
				newDevExt->DeviceNameBuffer,
				sizeof(newDevExt->DeviceNameBuffer) );
			SfGetObjectName( storageStackDeviceObject, 
				&newDevExt->DeviceName );
			//���һ�����Կ��Ƿ������Ѱ�������豸����
			//��������������ٴβ��� ���û�󶨽��а� ����������ԭ�Ӳ���
			ExAcquireFastMutex( &gSfilterAttachLock );
			if (!SfIsAttachedToDevice( devList[i], NULL )) 
			{
				//  �󶨵���
				status = SfAttachToMountedDevice( devList[i], 
					newDeviceObject );
				if (!NT_SUCCESS( status )) 
				{ 
					//��ʧ�� ���
					SfCleanupMountedDevice( newDeviceObject );
					IoDeleteDevice( newDeviceObject );
				}
				
			} 
			else 
			{
				//���ǰ��� �������豸����
				SfCleanupMountedDevice( newDeviceObject );
				IoDeleteDevice( newDeviceObject );
			}
			//�ͷ���
			ExReleaseFastMutex( &gSfilterAttachLock );
		} 
		finally 
		{
			if (storageStackDeviceObject != NULL) 
			{
				ObDereferenceObject( storageStackDeviceObject );
			}
			//���ټ��� IoEnumerateDeviceObjectList)
			ObDereferenceObject( devList[i] );
		}
	}
	//��ʱ���ǽ������κδ���  ���ִ���ʱ���Ǽ򵥵Ĳ�ȥ���κξ�
	status = STATUS_SUCCESS;
	// �ͷ����Ƿ���ı���ڴ�
	ExFreePoolWithTag( devList, SFLT_POOL_TAG );
	return status;
}


NTSTATUS
SfAttachToMountedDevice (
			 IN PDEVICE_OBJECT DeviceObject,
			 IN PDEVICE_OBJECT SFilterDeviceObject
			 )
{
	PSFILTER_DEVICE_EXTENSION newDevExt = SFilterDeviceObject->DeviceExtension;
	NTSTATUS status;
	ULONG i;
	PAGED_CODE();
	ASSERT(IS_MY_DEVICE_OBJECT( SFilterDeviceObject ));
	ASSERT(!SfIsAttachedToDevice ( DeviceObject, NULL ));
	// �豸��ǵĸ���
	if (FlagOn( DeviceObject->Flags, DO_BUFFERED_IO )) {
		SetFlag( SFilterDeviceObject->Flags, DO_BUFFERED_IO );
	}
	if (FlagOn( DeviceObject->Flags, DO_DIRECT_IO )) {
		SetFlag( SFilterDeviceObject->Flags, DO_DIRECT_IO );
	}
	// ѭ�����԰�.���п���ʧ�ܡ�����ܺ������û�ǡ����ͼ���������������Ĳ�������
	// mount ����dismount �й�.��������8 �γ����Աܿ���Щ�ɺ�.
	for (i=0; i < 8; i++) 
	{
		LARGE_INTEGER interval;
		status = SfAttachDeviceToDeviceStack( SFilterDeviceObject,
			DeviceObject,
			&newDevExt->AttachedToDeviceObject );
		if (NT_SUCCESS(status)) 
		{
			ClearFlag( SFilterDeviceObject->Flags, DO_DEVICE_INITIALIZING );
			return STATUS_SUCCESS;
		}
		// ������߳��ӳ�500 ������ټ���.
		interval.QuadPart = (500 * DELAY_ONE_MILLISECOND);
		KeDelayExecutionThread ( KernelMode, FALSE, &interval );
	}
	return status;
}


VOID
SfCleanupMountedDevice (
			IN PDEVICE_OBJECT DeviceObject
			)
//�����κα�Ҫ�Ĵ������豸��չ�е����� ���ͷ��ڴ�
{        
	
	UNREFERENCED_PARAMETER( DeviceObject );
	ASSERT(IS_MY_DEVICE_OBJECT( DeviceObject ));
}


VOID 
SfGetObjectName   ( 
		   IN   PVOID   Object, 
		   IN   OUT   PUNICODE_STRING   Name 
		   ) 
{ 
	NTSTATUS   status; 
	CHAR   nibuf[512];              //buffer   that   receives   NAME   information   and   name 
	POBJECT_NAME_INFORMATION  nameInfo=(POBJECT_NAME_INFORMATION)nibuf; 
	ULONG   retLength; 
	status=ObQueryNameString(Object,nameInfo,sizeof(nibuf),&retLength); 
	Name->Length=0; 
	if(NT_SUCCESS(status))
	{ 
		RtlCopyUnicodeString(Name,&nameInfo->Name); 
		KdPrint(("SfGetObjectName &nameInfo->Name=%wZ\n",&nameInfo-> Name));
	} 
}


VOID
SfGetBaseDeviceObjectName (
			   IN PDEVICE_OBJECT DeviceObject,//������Ҫ�����Ƶ��豸����ָ��
			   IN OUT PUNICODE_STRING Name//�Ѿ���ʼ�����Ļ����� ����豸��������
			   )
			   //λ�ڸ����İ����������ļ����� �����ض����� ���û���ҵ����� ���ؿ�
			   // �������λ���豸�����ڸ����ĸ�������Ȼ��
			   // ���ظö�������ơ� 
			   // ���û���κο����ҵ������֣�һ�����ַ������ء�
{
	//  ��û������ļ�ϵͳ�豸����
	ASSERT( NULL != gSfDynamicFunctions.GetDeviceAttachmentBaseRef );
	DeviceObject = (gSfDynamicFunctions.GetDeviceAttachmentBaseRef)( DeviceObject );
	//��ö�������
	SfGetObjectName( DeviceObject, Name );
	//���������IoGetDeviceAttachmentBaseRef�����ӵļ���
	ObDereferenceObject( DeviceObject );
}


PUNICODE_STRING
SfGetFileName(//����ļ��� ���ȷ����ʱ����һ���ɴ�ӡ���ַ�����Ҳ������NULL�� �����Ҫ������仺����
	      IN PFILE_OBJECT FileObject,
	      IN NTSTATUS CreateStatus,
	      IN OUT PGET_NAME_CONTROL NameControl
	      )
{
	POBJECT_NAME_INFORMATION nameInfo;
	NTSTATUS status;
	ULONG size;
	ULONG bufferSize;
	// ��û�з��仺����
	NameControl->allocatedBuffer = NULL;
	//�ýṹ����С�Ļ������������
	nameInfo = (POBJECT_NAME_INFORMATION)NameControl->smallBuffer;
	bufferSize = sizeof(NameControl->smallBuffer);
	//����򿪳ɹ� ����ļ��� ��ʧ�ܷ��ص��豸��
	status = ObQueryNameString(
		((NT_SUCCESS( CreateStatus ) ?(PVOID)FileObject:(PVOID)FileObject->DeviceObject)),
		nameInfo,
		bufferSize,
		&size );
	//�鿴�������Ƿ�С
	if (status == STATUS_INFO_LENGTH_MISMATCH) { //STATUS_BUFFER_OVERFLOW
		//С�Ļ��ͷ����㹻���
		bufferSize = size + sizeof(WCHAR);
		NameControl->allocatedBuffer = ExAllocatePoolWithTag( 
			NonPagedPool,
			bufferSize,
			SFLT_POOL_TAG );
   
		if (NULL == NameControl->allocatedBuffer) 
		{
			//����������ʧ�� ��������Ϊ�յ��ַ���
			RtlInitEmptyUnicodeString(
				(PUNICODE_STRING)&NameControl->smallBuffer,
				(PWCHAR)(NameControl->smallBuffer + sizeof(UNICODE_STRING)),
				(USHORT)(sizeof(NameControl->smallBuffer) - sizeof(UNICODE_STRING)) );
			return (PUNICODE_STRING)&NameControl->smallBuffer;
		}     
		RtlZeroMemory(NameControl->allocatedBuffer,bufferSize);
		//���÷���Ļ����� �ٴλ�ȡ����
		nameInfo = (POBJECT_NAME_INFORMATION)NameControl->allocatedBuffer;
		status = ObQueryNameString(
			FileObject,
			nameInfo,
			bufferSize,
			&size );
	}
	//�����ǻ��һ�����ֲ����ļ�������ô���ǻ�ȡ�豸��
	//���ļ������еõ����������ƣ�ע�� ֻ����Create������ʱ��ִ�У�
	// ֻ�����ǵ��Դ��󷵻�createʱ�ŷ��� 
	if (NT_SUCCESS( status ) && 
		!NT_SUCCESS( CreateStatus )) {
		ULONG newSize;
		PCHAR newBuffer;
		POBJECT_NAME_INFORMATION newNameInfo;
		//������Ҫ����������ϵĻ������Ĵ�С
		newSize = size + FileObject->FileName.Length;
		//�����ص��ļ��������� 
		//  If there is a related file object add in the length
		//  of that plus space for a separator
		//
		if (NULL != FileObject->RelatedFileObject) {
			newSize += FileObject->RelatedFileObject->FileName.Length + 
				sizeof(WCHAR);
		}
		//  See if it will fit in the existing buffer
		if (newSize > bufferSize) {
			//  It does not fit, allocate a bigger buffer
			newBuffer = ExAllocatePoolWithTag( 
				NonPagedPool,
				newSize,
				SFLT_POOL_TAG );
			if (NULL == newBuffer) 
			{
				// �����ڴ�ʧ�� ����һ�����ƵĿ��ַ���
				RtlInitEmptyUnicodeString(
					(PUNICODE_STRING)&NameControl->smallBuffer,
					(PWCHAR)(NameControl->smallBuffer + sizeof(UNICODE_STRING)),
					(USHORT)(sizeof(NameControl->smallBuffer) - sizeof(UNICODE_STRING)) );
				return (PUNICODE_STRING)&NameControl->smallBuffer;
			}
			RtlZeroMemory(newBuffer,newSize);
			//�þɵĻ����������ݳ�ʼ���µĻ����� 
			newNameInfo = (POBJECT_NAME_INFORMATION)newBuffer;
			RtlInitEmptyUnicodeString(
				&newNameInfo->Name,
				(PWCHAR)(newBuffer + sizeof(OBJECT_NAME_INFORMATION)),
				(USHORT)(newSize - sizeof(OBJECT_NAME_INFORMATION)) );
			RtlCopyUnicodeString( &newNameInfo->Name, 
				&nameInfo->Name );
			// �ͷžɵĻ����� Free the old allocated buffer (if there is one)
			//  and save off the new allocated buffer address.  It
			//  would be very rare that we should have to free the
			//  old buffer because device names should always fit
			//  inside it.
			if (NULL != NameControl->allocatedBuffer) 
			{
				ExFreePool( NameControl->allocatedBuffer );
			}
			//  Readjust our pointers
			NameControl->allocatedBuffer = newBuffer;
			bufferSize = newSize;
			nameInfo = newNameInfo;
		} else 
		{
			//  The MaximumLength was set by ObQueryNameString to
			//  one char larger then the length.  Set it to the
			//  true size of the buffer (so we can append the names)
			nameInfo->Name.MaximumLength = (USHORT)(bufferSize - 
				sizeof(OBJECT_NAME_INFORMATION));
		}
		//�������ص��ļ����� ���ȸ������Ƶ����豸����with a �ָ���
		if (NULL != FileObject->RelatedFileObject) 
		{
			RtlAppendUnicodeStringToString(
				&nameInfo->Name,
				&FileObject->RelatedFileObject->FileName );
			RtlAppendUnicodeToString( &nameInfo->Name, L"\\" );
		}
		//�ļ����󸽼�����
		RtlAppendUnicodeStringToString(
			&nameInfo->Name,
			&FileObject->FileName );
		ASSERT(nameInfo->Name.Length <= nameInfo->Name.MaximumLength);//���ƴ�Сû�����
	}
	//	��������
	return &nameInfo->Name;
}


VOID
SfGetFileNameCleanup(//�鿴�������Ƿ񱻷��� ���ǵĻ����ͷ�
		     IN OUT PGET_NAME_CONTROL NameControl//���ڼ������ƵĿ��ƽṹ
		     )
{
	if (NULL != NameControl->allocatedBuffer) {
		ExFreePool( NameControl->allocatedBuffer);
		NameControl->allocatedBuffer = NULL;
	}
}



BOOLEAN
SfIsAttachedToDevice (
		      PDEVICE_OBJECT DeviceObject,
		      PDEVICE_OBJECT *AttachedDeviceObject OPTIONAL
		      )
{
	PDEVICE_OBJECT currentDevObj;
	PDEVICE_OBJECT nextDevObj;
	PAGED_CODE();
	// 	ASSERT( NULL != gSfDynamicFunctions.GetLowerDeviceObject &&
	// 		NULL != gSfDynamicFunctions.GetDeviceAttachmentBaseRef );
	// ��ð���������豸���� 
	ASSERT(NULL != gSfDynamicFunctions.GetAttachedDeviceReference);
	currentDevObj = (gSfDynamicFunctions.GetAttachedDeviceReference)(DeviceObject);
	do {//���±������� �ҵ����ǵ��豸����
		if (IS_MY_DEVICE_OBJECT( currentDevObj ))
		{
			if (ARGUMENT_PRESENT(AttachedDeviceObject))
			{//�ж��Ƿ�ΪNULL
				*AttachedDeviceObject = currentDevObj;
			} 
			else 
			{
				ObDereferenceObject( currentDevObj );
			}
			return TRUE;
		}
		//  �����һ���󶨵Ķ��� Get the next attached object.  This puts a reference on 
		//  the device object.
		ASSERT( NULL != gSfDynamicFunctions.GetLowerDeviceObject );
		nextDevObj = (gSfDynamicFunctions.GetLowerDeviceObject)( currentDevObj );
		//ָ����һ��ǰ����������  
		ObDereferenceObject( currentDevObj );
		currentDevObj = nextDevObj;
	} while (NULL != currentDevObj);
	//û���ڰ����Ϸ��������Լ��� ���ؿ� �ͷ���û���ҵ� 
	if (ARGUMENT_PRESENT(AttachedDeviceObject)) 
	{//����Ϊ�� ������Ϊ��
		*AttachedDeviceObject = NULL;
	}
	return FALSE;
}    



NTSTATUS
SfIsShadowCopyVolume (//�鿴StorageStackDeviceObject�Ƿ�Ϊ��Ӱ
		      IN PDEVICE_OBJECT StorageStackDeviceObject,
		      OUT PBOOLEAN IsShadowCopy
		      )
{
	PAGED_CODE();
	*IsShadowCopy = FALSE;

	if (IS_WINDOWSXP()) 
	{
		UNICODE_STRING volSnapDriverName;
		WCHAR buffer[MAX_DEVNAME_LENGTH];
		PUNICODE_STRING storageDriverName;
		ULONG returnedLength;
		NTSTATUS status;
		//  In Windows XP���еľ�Ӱ���Ͷ���FILE_DISK_DEVICE.��֮�򲻳���
		if (FILE_DEVICE_DISK != StorageStackDeviceObject->DeviceType) 
		{
			return STATUS_SUCCESS;
		}
		//  ��Ҫ�鿴���������Ƿ�Ϊ \Driver\VolSnap 
		storageDriverName = (PUNICODE_STRING) buffer;
		RtlInitEmptyUnicodeString( storageDriverName, 
			Add2Ptr( storageDriverName, sizeof( UNICODE_STRING ) ),
			sizeof( buffer ) - sizeof( UNICODE_STRING ) );
		status = ObQueryNameString( StorageStackDeviceObject,
			(POBJECT_NAME_INFORMATION)storageDriverName,
			storageDriverName->MaximumLength,
			&returnedLength );
		if (!NT_SUCCESS( status )) 
		{
			return status;
		}
		RtlInitUnicodeString( &volSnapDriverName, L"\\Driver\\VolSnap" );
		if (RtlEqualUnicodeString( storageDriverName, &volSnapDriverName, TRUE ))
		{
			//�Ǿ�Ӱ �������÷���ֵ�� 
			*IsShadowCopy = TRUE;
		} else 
		{
			// ���Ǿ�Ӱ
			NOTHING;
		}
		
	} 
	return STATUS_SUCCESS;
}


//
//sfilter�ṩ�������ص������ӿ�
//
NTSTATUS
SfPreFsFilterPassThrough(
			 IN PFS_FILTER_CALLBACK_DATA Data,
			 OUT PVOID *CompletionContext
			 )
{
	UNREFERENCED_PARAMETER( Data );
	UNREFERENCED_PARAMETER( CompletionContext );
	
	ASSERT( IS_MY_DEVICE_OBJECT( Data->DeviceObject ) );
	
	return STATUS_SUCCESS;
}

VOID
SfPostFsFilterPassThrough (
			   IN PFS_FILTER_CALLBACK_DATA Data,
			   IN NTSTATUS OperationStatus,
			   IN PVOID CompletionContext
			   )
{
	UNREFERENCED_PARAMETER( Data );
	UNREFERENCED_PARAMETER( OperationStatus );
	UNREFERENCED_PARAMETER( CompletionContext );
	
	ASSERT( IS_MY_DEVICE_OBJECT( Data->DeviceObject ) );
}




NTSTATUS
SfQuerySymbolicLink(
					IN  PUNICODE_STRING SymbolicLinkName,
					OUT PUNICODE_STRING LinkTarget
					)			  
//�������ڻ�ȡ�����������ĺ���									
{
    HANDLE Handle;
    OBJECT_ATTRIBUTES ObjAttribute;
    NTSTATUS Status;

	//��ʼ��OBJECT_ATTRIBUTES�ṹ�� 
    InitializeObjectAttributes(
							   &ObjAttribute, 
							   SymbolicLinkName, 
							   OBJ_CASE_INSENSITIVE,
							   0, 
							   0);
	
	//��ͼ���豸  ��ȡ���
    Status = ZwOpenSymbolicLinkObject(&Handle, GENERIC_READ, &ObjAttribute);
    if (!NT_SUCCESS(Status))
        return Status;
	
    LinkTarget->MaximumLength = 200*sizeof(WCHAR);
    LinkTarget->Length = 0;
    LinkTarget->Buffer = ExAllocatePool(PagedPool, LinkTarget->MaximumLength);
    if (!LinkTarget->Buffer)
    {
        ZwClose(Handle);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
	
	//ͨ�������ȡ������
    Status = ZwQuerySymbolicLinkObject(Handle, LinkTarget, NULL);
    ZwClose(Handle);
	
    if (!NT_SUCCESS(Status))
        ExFreePool(LinkTarget->Buffer);
	
    return Status;
}

NTSTATUS
SfVolumeDeviceNameToDosName(
							IN PUNICODE_STRING VolumeDeviceName,
							OUT PUNICODE_STRING DosName
							)
//�����������dosname��������Ҫʱmust call ExFreePool on DosName->Buffer																	
{
    WCHAR Buffer[30]=L"\\??\\C:";
    UNICODE_STRING DriveLetterName;
    UNICODE_STRING LinkTarget;
    WCHAR Char;
    NTSTATUS Status;


    RtlInitUnicodeString(&DriveLetterName, Buffer);
	
    for (Char = 'A'; Char <= 'Z'; Char++)
    {
        DriveLetterName.Buffer[4] = Char;
		
        Status = SfQuerySymbolicLink(&DriveLetterName, &LinkTarget);
        if (!NT_SUCCESS(Status))
            continue;
		
        if (RtlEqualUnicodeString(&LinkTarget, VolumeDeviceName, TRUE))
        {
            ExFreePool(LinkTarget.Buffer);
            break;
        }
		
        ExFreePool(LinkTarget.Buffer);
    }
	
    if (Char <= 'Z')
    {
        DosName->Buffer = ExAllocatePool(PagedPool, 3*sizeof(WCHAR));
        if (!DosName->Buffer)
            return STATUS_INSUFFICIENT_RESOURCES;
		
        DosName->MaximumLength = 6;
        DosName->Length = 4;
        DosName->Buffer[0] = Char;
        DosName->Buffer[1] = ':';
        DosName->Buffer[2] = 0;
		
        return STATUS_SUCCESS;
    }
	
    return STATUS_UNSUCCESSFUL;
}