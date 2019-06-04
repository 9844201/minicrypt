///////////////////////////////////ͷ�ļ�//////////////////////////////////////////////

#include "crypt.h"
#include "fat_headers/fat.h"
#include "fat_headers/nodetype.h"
#include "fat_headers/fatstruc.h"

//////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
#define CF_FILE_HEADER_SIZE (1024*4)
#define CF_MEM_TAG 'cfmi'

static LIST_ENTRY s_cf_list;                        //�����б�
static KSPIN_LOCK s_cf_list_lock;                   //���ü��ܱ��ʱ���������
static KIRQL s_cf_list_lock_irql;                   //���ܱ���жϼ�
static BOOLEAN s_cf_list_inited = FALSE;            //�����б��Ƿ��Ѿ���ʼ��v
static size_t s_cf_proc_name_offset = 0;            //��¼��������ƫ����

//�����ÿ�������ļ��ĵ�һ���ڵ㣬Ҳ���Ǽ�������
typedef struct {
    LIST_ENTRY list_entry;                //˫���б�  ������
    FCB *fcb;                             //��Ӧ��һ���ļ�
} CF_NODE,*PCF_NODE;

// д���������ġ���Ϊд�������ָ�ԭ����irp->MdlAddress
// ����irp->UserBuffer�����Բ���Ҫ��¼�����ġ�
//����ṹ����ָ�������ģ���Ϊд�ļ��Ǳ����滻�����������Լ�����ģ�
//����������������ԭ���������ģ������������Իظ�
typedef struct CF_WRITE_CONTEXT_{
    PMDL mdl_address;
    PVOID user_buffer;
} CF_WRITE_CONTEXT,*PCF_WRITE_CONTEXT;
/////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////cf_proc.c////////////////////////////////////
// �������������DriverEntry�е��ã�����cfCurProcName���������á�
//�����������ȫ�ֱ����б�����̵�ƫ��λ��
void cfCurProcNameInit()
{
	ULONG i;
	PEPROCESS  curproc;
	curproc = PsGetCurrentProcess();       //��ʼ��ʱ��ǰ������System������
	// ����EPROCESS�ṹ���������ҵ��ַ���
	for(i=0;i<3*4*1024;i++)
	{
		if(!strncmp("System",(PCHAR)curproc+i,strlen("System"))) 
		{
			s_cf_proc_name_offset = i;
			break;
		}
	}
}

// ���º������Ի�ý����������ػ�õĳ��ȡ�
ULONG cfCurProcName(PUNICODE_STRING name)
{
	PEPROCESS  curproc;
	ULONG	need_len;
    ANSI_STRING ansi_name;
	if(s_cf_proc_name_offset == 0)
		return 0;
	
    // ��õ�ǰ����PEB,Ȼ���ƶ�һ��ƫ�Ƶõ�����������λ�á�
	curproc = PsGetCurrentProcess();
	
    // ���������ansi�ַ���������ת��Ϊunicode�ַ�����
    RtlInitAnsiString(&ansi_name,((PCHAR)curproc + s_cf_proc_name_offset));
    need_len = RtlAnsiStringToUnicodeSize(&ansi_name);
    if(need_len > name->MaximumLength)
    {
        return RtlAnsiStringToUnicodeSize(&ansi_name);
    }
    RtlAnsiStringToUnicodeString(name,&ansi_name,FALSE);
	return need_len;
}

// �жϵ�ǰ�����ǲ���notepad.exe
BOOLEAN cfIsCurProcSec(void)
{
    WCHAR name_buf[32] = { 0 };
    UNICODE_STRING proc_name = { 0 };
    UNICODE_STRING note_pad = { 0 };
    ULONG length;
    RtlInitEmptyUnicodeString(&proc_name,name_buf,32*sizeof(WCHAR));
    length = cfCurProcName(&proc_name);
    RtlInitUnicodeString(&note_pad,L"notepad.exe");
    if(RtlCompareUnicodeString(&note_pad,&proc_name,TRUE) == 0)
        return TRUE;
    return FALSE;
}

////////////////////////////////////////////cf_proc.c///////////////////////////////



///////////////////////////////////////////cf_modify_irp.c//////////////////////////
//���ǵ�irp��Ϊirpsp->MajorFunction == IRP_MJ_SET_INFORMATION�ǵ��õ�
// ����Щset information�����޸ģ�ʹ֮��ȥǰ���4k�ļ�ͷ��
void cfIrpSetInforPre(
    PIRP irp,
    PIO_STACK_LOCATION irpsp)
{
    PUCHAR buffer = irp->AssociatedIrp.SystemBuffer;
    //NTSTATUS status;

    ASSERT(irpsp->MajorFunction == IRP_MJ_SET_INFORMATION);
    switch(irpsp->Parameters.SetFile.FileInformationClass)
    {
		//������Ĳ�������4k
    case FileAllocationInformation:
        {
		    PFILE_ALLOCATION_INFORMATION alloc_infor = 
                (PFILE_ALLOCATION_INFORMATION)buffer;

		    alloc_infor->AllocationSize.QuadPart += CF_FILE_HEADER_SIZE;        
            break;
        }
    case FileEndOfFileInformation:
        {
		    PFILE_END_OF_FILE_INFORMATION end_infor = 
                (PFILE_END_OF_FILE_INFORMATION)buffer;
		    end_infor->EndOfFile.QuadPart += CF_FILE_HEADER_SIZE;
            break;
        }
    case FileValidDataLengthInformation:
        {
		    PFILE_VALID_DATA_LENGTH_INFORMATION valid_length = 
                (PFILE_VALID_DATA_LENGTH_INFORMATION)buffer;
		    valid_length->ValidDataLength.QuadPart += CF_FILE_HEADER_SIZE;
            break;
        }
	case FilePositionInformation:
		{
			PFILE_POSITION_INFORMATION position_infor = 
				(PFILE_POSITION_INFORMATION)buffer;
			position_infor->CurrentByteOffset.QuadPart += CF_FILE_HEADER_SIZE;
			break;
		}
	case FileStandardInformation:
		((PFILE_STANDARD_INFORMATION)buffer)->EndOfFile.QuadPart += CF_FILE_HEADER_SIZE;
		break;
	case FileAllInformation:
		((PFILE_ALL_INFORMATION)buffer)->PositionInformation.CurrentByteOffset.QuadPart += CF_FILE_HEADER_SIZE;
		((PFILE_ALL_INFORMATION)buffer)->StandardInformation.EndOfFile.QuadPart += CF_FILE_HEADER_SIZE;
		break;

    default:
        ASSERT(FALSE);
    };
}

//��Ϊ�����ļ���Ҫ���ļ�ͷ���ż��ܱ�־�����Ա��������ļ�ͷ����������
//IRP_MJ_QUERY_INFORMATION��ѯ�ļ����õ���Ϳ��Խ����޸���
void cfIrpQueryInforPost(PIRP irp,PIO_STACK_LOCATION irpsp)
{
    PUCHAR buffer = irp->AssociatedIrp.SystemBuffer;
    ASSERT(irpsp->MajorFunction == IRP_MJ_QUERY_INFORMATION);
    switch(irpsp->Parameters.QueryFile.FileInformationClass)
    {
    case FileAllInformation:
        {
            // ע��FileAllInformation���������½ṹ��ɡ���ʹ���Ȳ�����
            // ��Ȼ���Է���ǰ����ֽڡ�
            //typedef struct _FILE_ALL_INFORMATION {
            //    FILE_BASIC_INFORMATION BasicInformation;........................
            //    FILE_STANDARD_INFORMATION StandardInformation;..................
            //    FILE_INTERNAL_INFORMATION InternalInformation;
            //    FILE_EA_INFORMATION EaInformation;
            //    FILE_ACCESS_INFORMATION AccessInformation;
            //    FILE_POSITION_INFORMATION PositionInformation;
            //    FILE_MODE_INFORMATION ModeInformation;
            //    FILE_ALIGNMENT_INFORMATION AlignmentInformation;
            //    FILE_NAME_INFORMATION NameInformation;
            //} FILE_ALL_INFORMATION, *PFILE_ALL_INFORMATION;
            // ������Ҫע����Ƿ��ص��ֽ����Ƿ������StandardInformation
            // �������Ӱ���ļ��Ĵ�С����Ϣ��
            PFILE_ALL_INFORMATION all_infor = (PFILE_ALL_INFORMATION)buffer;
            if(irp->IoStatus.Information >= 
                sizeof(FILE_BASIC_INFORMATION) + 
                sizeof(FILE_STANDARD_INFORMATION))
            {
				//ȷ���ļ�������λ�ã�����4k���ļ�ͷ
                ASSERT(all_infor->StandardInformation.EndOfFile.QuadPart >= CF_FILE_HEADER_SIZE);
				//�Ѳ�ѯ�����ļ���Ϣ��ȥ�ļ�ͷ
				all_infor->StandardInformation.EndOfFile.QuadPart -= CF_FILE_HEADER_SIZE;
                all_infor->StandardInformation.AllocationSize.QuadPart -= CF_FILE_HEADER_SIZE;
                if(irp->IoStatus.Information >= 
                    sizeof(FILE_BASIC_INFORMATION) + 
                    sizeof(FILE_STANDARD_INFORMATION) +
                    sizeof(FILE_INTERNAL_INFORMATION) +
                    sizeof(FILE_EA_INFORMATION) +
                    sizeof(FILE_ACCESS_INFORMATION) +
                    sizeof(FILE_POSITION_INFORMATION))
                {
                    if(all_infor->PositionInformation.CurrentByteOffset.QuadPart >= CF_FILE_HEADER_SIZE)
                        all_infor->PositionInformation.CurrentByteOffset.QuadPart -= CF_FILE_HEADER_SIZE;
                }
            }
            break;
        }
    case FileAllocationInformation:
        {
		    PFILE_ALLOCATION_INFORMATION alloc_infor = 
                (PFILE_ALLOCATION_INFORMATION)buffer;
            ASSERT(alloc_infor->AllocationSize.QuadPart >= CF_FILE_HEADER_SIZE);
		    alloc_infor->AllocationSize.QuadPart -= CF_FILE_HEADER_SIZE;        
            break;
        }
    case FileValidDataLengthInformation:
        {
		    PFILE_VALID_DATA_LENGTH_INFORMATION valid_length = 
                (PFILE_VALID_DATA_LENGTH_INFORMATION)buffer;
            ASSERT(valid_length->ValidDataLength.QuadPart >= CF_FILE_HEADER_SIZE);
		    valid_length->ValidDataLength.QuadPart -= CF_FILE_HEADER_SIZE;
            break;
        }
    case FileStandardInformation:
        {
            PFILE_STANDARD_INFORMATION stand_infor = (PFILE_STANDARD_INFORMATION)buffer;
            ASSERT(stand_infor->AllocationSize.QuadPart >= CF_FILE_HEADER_SIZE);
            stand_infor->AllocationSize.QuadPart -= CF_FILE_HEADER_SIZE;            
            stand_infor->EndOfFile.QuadPart -= CF_FILE_HEADER_SIZE;
            break;
        }
    case FileEndOfFileInformation:
        {
		    PFILE_END_OF_FILE_INFORMATION end_infor = 
                (PFILE_END_OF_FILE_INFORMATION)buffer;
            ASSERT(end_infor->EndOfFile.QuadPart >= CF_FILE_HEADER_SIZE);
		    end_infor->EndOfFile.QuadPart -= CF_FILE_HEADER_SIZE;
            break;
        }
	case FilePositionInformation:
		{
			PFILE_POSITION_INFORMATION PositionInformation =
				(PFILE_POSITION_INFORMATION)buffer; 
            if(PositionInformation->CurrentByteOffset.QuadPart > CF_FILE_HEADER_SIZE)
			    PositionInformation->CurrentByteOffset.QuadPart -= CF_FILE_HEADER_SIZE;
			break;
		}
    default:
        ASSERT(FALSE);
    };
}

// �����󡣽�ƫ����ǰ�ơ�
void cfIrpReadPre(PIRP irp,PIO_STACK_LOCATION irpsp)
{
    PLARGE_INTEGER offset;
    PFCB fcb = (PFCB)irpsp->FileObject->FsContext;
	offset = &irpsp->Parameters.Read.ByteOffset;

	UNREFERENCED_PARAMETER(irp);
    if(offset->LowPart ==  FILE_USE_FILE_POINTER_POSITION &&  offset->HighPart == -1)
	{
		//���������ָ��irp���ܲ���ȷ�������ƫ�ƣ�����Ҫ�󰴵�ǰƫ���������
        // ���±�������������������
        ASSERT(FALSE);
	}
    // ƫ�Ʊ����޸�Ϊ����4k��
    offset->QuadPart += CF_FILE_HEADER_SIZE;
    KdPrint(("cfIrpReadPre: offset = %8x\r\n",
        offset->LowPart));
}

// �������������Ҫ���ܡ�������ʱ�Ľ��������д �Ƚϼ�
// д����ļ��ܣ�Ҫ�Լ��趨����������ֹ�ظ���������
void cfIrpReadPost(PIRP irp,PIO_STACK_LOCATION irpsp)
{
    // �õ���������Ȼ�����֮�����ܼܺ򵥣�����xor 0x77.
    PUCHAR buffer;
    ULONG i,length = irp->IoStatus.Information;

	UNREFERENCED_PARAMETER(irpsp);
    ASSERT(irp->MdlAddress != NULL || irp->UserBuffer != NULL);
	//�ж����������ĸ������У�������ȡ����
	if(irp->MdlAddress != NULL)
		buffer = MmGetSystemAddressForMdlSafe(irp->MdlAddress,NormalPagePriority);
	else
		buffer = irp->UserBuffer;

    // ����Ҳ�ܼ򵥣�xor 0x77  �ӽ���һ���԰˸��ֽ�Ϊһ�飬Ҳ����һҳ
    for(i=0;i<length;++i)
        buffer[i] ^= 0X77;
    // ��ӡ����֮�������
    KdPrint(("cfIrpReadPost: flags = %x length = %x content = %c%c%c%c%c\r\n",
        irp->Flags,length,buffer[0],buffer[1],buffer[2],buffer[3],buffer[4]));
}

//��Ӧ����Ϊ�����д���ܣ���д�ļ�������mdl�ĺ���
// ����һ��MDL������һ������Ϊlength�Ļ�������
PMDL cfMdlMemoryAlloc(ULONG length)
{
    void *buf = ExAllocatePoolWithTag(NonPagedPool,length,CF_MEM_TAG);
    PMDL mdl;
    if(buf == NULL)
        return NULL;
    mdl = IoAllocateMdl(buf,length,FALSE,FALSE,NULL);
    if(mdl == NULL)
    {
		//���ʧ�ܣ����ͷ���������Ļ�����
        ExFreePool(buf);
        return NULL;
    }
	//����mdl��ҳ�����ڴ��һ�黺����
    MmBuildMdlForNonPagedPool(mdl);
    mdl->Next = NULL;
    return mdl;
}

// �ͷŵ�����MDL�Ļ�������
void cfMdlMemoryFree(PMDL mdl)
{
    void *buffer = MmGetSystemAddressForMdlSafe(mdl,NormalPagePriority);
    IoFreeMdl(mdl);
    ExFreePool(buffer);
}



// д������Ҫ���·��仺�����������п���ʧ�ܡ����ʧ��
// �˾�ֱ�ӱ����ˡ�����Ҫ��һ�����ء�TRUE��ʾ�ɹ�����
// �Լ���GO_ON��FALSE��ʾʧ���ˣ������Ѿ���ã�ֱ��
// ��ɼ���
BOOLEAN cfIrpWritePre(PIRP irp,PIO_STACK_LOCATION irpsp, PVOID *context)
{
    PLARGE_INTEGER offset;
    ULONG i,length = irpsp->Parameters.Write.Length;
    PUCHAR buffer,new_buffer;
    PMDL new_mdl = NULL;

    // ��׼��һ��������
    PCF_WRITE_CONTEXT my_context = (PCF_WRITE_CONTEXT)
    ExAllocatePoolWithTag(NonPagedPool,sizeof(CF_WRITE_CONTEXT),CF_MEM_TAG);
    if(my_context == NULL)
    {
        irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        irp->IoStatus.Information = 0;
        return FALSE;
    }
  
    // ������õ�������м��ܡ�Ҫע�����д����Ļ�����
    // �ǲ�����ֱ�Ӹ�д�ġ��������·��䡣
    ASSERT(irp->MdlAddress != NULL || irp->UserBuffer != NULL);
	if(irp->MdlAddress != NULL)
    {
		//buffer����ԭ���Ļ�����
		buffer = MmGetSystemAddressForMdlSafe(irp->MdlAddress,NormalPagePriority);
        new_mdl = cfMdlMemoryAlloc(length);
        if(new_mdl == NULL)
            new_buffer = NULL;
        else
            new_buffer = MmGetSystemAddressForMdlSafe(new_mdl,NormalPagePriority);
    }
	else
    {
		buffer = irp->UserBuffer;
        new_buffer = ExAllocatePoolWithTag(NonPagedPool,length,CF_MEM_TAG);
    }
    // �������������ʧ���ˣ�ֱ���˳����ɡ�
    if(new_buffer == NULL)
    {
        irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        irp->IoStatus.Information = 0;
        ExFreePool(my_context);
        return FALSE;
    }
	//��ԭ�����������Ƶ��»�����
    RtlCopyMemory(new_buffer,buffer,length);

    // ��������һ���ɹ������������������ˡ�
	//��ԭ����mdl��UserBuffer���������ģ��������ڲ�����
    my_context->mdl_address = irp->MdlAddress;
    my_context->user_buffer = irp->UserBuffer;
    *context = (void *)my_context;

    // ��irpָ���е�mdl�������֮���ٻָ�������
	//��Ҫ�����Ŀռ�ָ�����������
    if(new_mdl == NULL)
        irp->UserBuffer = new_buffer;
    else
        irp->MdlAddress = new_mdl;

	offset = &irpsp->Parameters.Write.ByteOffset;
    KdPrint(("cfIrpWritePre: fileobj = %x flags = %x offset = %8x length = %x content = %c%c%c%c%c\r\n",
        irpsp->FileObject,irp->Flags,offset->LowPart,length,buffer[0],buffer[1],buffer[2],buffer[3],buffer[4]));

    // ����Ҳ�ܼ򵥣�xor 0x77  ��������Ļ����м���
    for(i=0;i<length;++i)
        new_buffer[i] ^= 0x77;

    if(offset->LowPart ==  FILE_USE_FILE_POINTER_POSITION &&  offset->HighPart == -1)
	{
		//���������ָдirp���ܲ���ȷ����д��ƫ�ƣ�����Ҫ�󰴵�ǰƫ���������
        // ���±�������������������
        ASSERT(FALSE);
	}
    // ƫ�Ʊ����޸�Ϊ����4KB��
    offset->QuadPart += CF_FILE_HEADER_SIZE;
    return TRUE;
}

// ��ע�����۽����Σ����������WritePost.���������޷��ָ�
// Write�����ݣ��ͷ��ѷ���Ŀռ�������
void cfIrpWritePost(PIRP irp,PIO_STACK_LOCATION irpsp,void *context)
{
    PCF_WRITE_CONTEXT my_context = (PCF_WRITE_CONTEXT) context;

	UNREFERENCED_PARAMETER(irpsp);
    // ����������Իָ�irp�������ˡ�
    if(irp->MdlAddress != NULL)
        cfMdlMemoryFree(irp->MdlAddress);
    if(irp->UserBuffer != NULL)
        ExFreePool(irp->UserBuffer);
    irp->MdlAddress = my_context->mdl_address;
    irp->UserBuffer = my_context->user_buffer;
    ExFreePool(my_context);
}

//////////////////////////////////////////cf_modify_irp.c///////////////////////////


///////////////////////////////////////////cf_list.c////////////////////////////////


BOOLEAN cfListInited()
{
    return s_cf_list_inited;
}
 
void cfListLock()
{
    ASSERT(s_cf_list_inited);
    KeAcquireSpinLock(&s_cf_list_lock,&s_cf_list_lock_irql);
}

void cfListUnlock()
{
    ASSERT(s_cf_list_inited);
    KeReleaseSpinLock(&s_cf_list_lock,s_cf_list_lock_irql);
}

void cfListInit()
{
    InitializeListHead(&s_cf_list);
    KeInitializeSpinLock(&s_cf_list_lock);
    s_cf_list_inited = TRUE;
}

// �������һ���ļ����ж��Ƿ��ڼ��������С��������û������
BOOLEAN cfIsFileCrypting(PFILE_OBJECT file)
{
    PLIST_ENTRY p;
    PCF_NODE node;
   for(p = s_cf_list.Flink; p != &s_cf_list; p = p->Flink)
    {
	    node = (PCF_NODE)p;
        if(node->fcb == file->FsContext)
        {
            //KdPrint(("cfIsFileCrypting: file %wZ is crypting. fcb = %x \r\n",&file->FileName,file->FsContext));
            return TRUE;
        }
    } 
    return FALSE;
}

// ׷��һ������ʹ�õĻ����ļ�����������м�������ֻ֤����һ
// ���������ظ����롣
BOOLEAN cfFileCryptAppendLk(PFILE_OBJECT file)
{
    // �ȷ���ռ�
    PCF_NODE node = (PCF_NODE)
        ExAllocatePoolWithTag(NonPagedPool,sizeof(CF_NODE),CF_MEM_TAG);
    node->fcb = (PFCB)file->FsContext;

    cfFileCacheClear(file);    //���������

    // ���������ң�����Ѿ����ˣ�����һ�������Ĵ���ֱ�ӱ����ɡ�
    cfListLock();
    if(cfIsFileCrypting(file))
    {
        ASSERT(FALSE);
        return TRUE;
    }
    else if(node->fcb->UncleanCount > 1)
    {
        // Ҫ�ɹ��ļ��룬����Ҫ����һ������������FCB->UncleanCount <= 1.
        // �����Ļ�˵��û�����������������ļ�������Ļ�������һ����
        // ͨ���̴���������ʱ���ܼ��ܡ����ؾܾ��򿪡�
        cfListUnlock();
        // �ͷŵ���
        ExFreePool(node);
        return FALSE;
    }

    // ����Ļ�����������뵽�����
    InsertHeadList(&s_cf_list, (PLIST_ENTRY)node);
    cfListUnlock();

    //cfFileCacheClear(file);
    return TRUE;
}


// �����ļ���clean up��ʱ����ô˺����������鷢��
// FileObject->FsContext���б���
BOOLEAN cfCryptFileCleanupComplete(PFILE_OBJECT file)
{
    PLIST_ENTRY p;
    PCF_NODE node;
    FCB *fcb = (FCB *)file->FsContext;

    KdPrint(("cfCryptFileCleanupComplete: file name = %wZ, fcb->UncleanCount = %d\r\n",
        &file->FileName,fcb->UncleanCount));

    // �����������ļ����塣Ȼ���ٴ��������Ƴ�������Ļ����建
    // ��ʱ��д�����Ͳ�������ˡ�
    if(fcb->UncleanCount <= 1 || (fcb->FcbState & FCB_STATE_DELETE_ON_CLOSE) )
        cfFileCacheClear(file);
    else
        return FALSE;

    cfListLock();
   for(p = s_cf_list.Flink; p != &s_cf_list; p = p->Flink)
   {
	    node = (PCF_NODE)p;
        if(node->fcb == file->FsContext && 
            (node->fcb->UncleanCount == 0 ||
            (fcb->FcbState & FCB_STATE_DELETE_ON_CLOSE)))
        {
            // ���������Ƴ���
            RemoveEntryList((PLIST_ENTRY)node);
            cfListUnlock();
            //  �ͷ��ڴ档
            ExFreePool(node);
            return TRUE;
        }
    } 
    cfListUnlock();
   return FALSE;
}

///////////////////////////////////////////cf_list.c////////////////////////////////



//////////////////////////////////////////cf_file_irp.c////////////////////////////



static NTSTATUS cfFileIrpComp(
    PDEVICE_OBJECT dev,
    PIRP irp,
    PVOID context
    )
{
	UNREFERENCED_PARAMETER(context);
	UNREFERENCED_PARAMETER(dev);

    *irp->UserIosb = irp->IoStatus;
    KeSetEvent(irp->UserEvent, 0, FALSE);
    IoFreeIrp(irp);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

// �Է���SetInformation����.
NTSTATUS 
cfFileSetInformation( 
    DEVICE_OBJECT *dev, 
    FILE_OBJECT *file,
    FILE_INFORMATION_CLASS infor_class,
	FILE_OBJECT *set_file,
    void* buf,
    ULONG buf_len)
{
    PIRP irp;
    KEVENT event;
    IO_STATUS_BLOCK IoStatusBlock;
    PIO_STACK_LOCATION ioStackLocation;

    KeInitializeEvent(&event, SynchronizationEvent, FALSE);

	// ����irp
    irp = IoAllocateIrp(dev->StackSize, FALSE);
    if(irp == NULL)
        return STATUS_INSUFFICIENT_RESOURCES;

	// ��дirp������
    irp->AssociatedIrp.SystemBuffer = buf;
    irp->UserEvent = &event;
    irp->UserIosb = &IoStatusBlock;
    irp->Tail.Overlay.Thread = PsGetCurrentThread();
    irp->Tail.Overlay.OriginalFileObject = file;
    irp->RequestorMode = KernelMode;
    irp->Flags = 0;

	// ����irpsp
    ioStackLocation = IoGetNextIrpStackLocation(irp);
    ioStackLocation->MajorFunction = IRP_MJ_SET_INFORMATION;
    ioStackLocation->DeviceObject = dev;
    ioStackLocation->FileObject = file;
    ioStackLocation->Parameters.SetFile.FileObject = set_file;
    ioStackLocation->Parameters.SetFile.Length = buf_len;
    ioStackLocation->Parameters.SetFile.FileInformationClass = infor_class;

	// ���ý�������
    IoSetCompletionRoutine(irp, cfFileIrpComp, 0, TRUE, TRUE, TRUE);

	// �������󲢵ȴ�����
    (void) IoCallDriver(dev, irp);
    KeWaitForSingleObject(&event, Executive, KernelMode, TRUE, 0);
    return IoStatusBlock.Status;
}

NTSTATUS
cfFileQueryInformation(
    DEVICE_OBJECT *dev, 
    FILE_OBJECT *file,
    FILE_INFORMATION_CLASS infor_class,
    void* buf,
    ULONG buf_len)
{
    PIRP irp;
    KEVENT event;
    IO_STATUS_BLOCK IoStatusBlock;
    PIO_STACK_LOCATION ioStackLocation;

    // ��Ϊ���Ǵ������������ͬ����ɣ����Գ�ʼ��һ���¼�
    // �����ȴ�������ɡ�
    KeInitializeEvent(&event, SynchronizationEvent, FALSE);

	// ����irp
    irp = IoAllocateIrp(dev->StackSize, FALSE);
    if(irp == NULL)
        return STATUS_INSUFFICIENT_RESOURCES;

	// ��дirp������
    irp->AssociatedIrp.SystemBuffer = buf;
    irp->UserEvent = &event;
    irp->UserIosb = &IoStatusBlock;
    irp->Tail.Overlay.Thread = PsGetCurrentThread();
    irp->Tail.Overlay.OriginalFileObject = file;
    irp->RequestorMode = KernelMode;
    irp->Flags = 0;

	// ����irpsp
    ioStackLocation = IoGetNextIrpStackLocation(irp);
    ioStackLocation->MajorFunction = IRP_MJ_QUERY_INFORMATION;
    ioStackLocation->DeviceObject = dev;
    ioStackLocation->FileObject = file;
    ioStackLocation->Parameters.QueryFile.Length = buf_len;
    ioStackLocation->Parameters.QueryFile.FileInformationClass = infor_class;

	// ���ý�������
    IoSetCompletionRoutine(irp, cfFileIrpComp, 0, TRUE, TRUE, TRUE);

	// �������󲢵ȴ�����
    (void) IoCallDriver(dev, irp);
    KeWaitForSingleObject(&event, Executive, KernelMode, TRUE, 0);
    return IoStatusBlock.Status;
}

NTSTATUS
cfFileSetFileSize(
	DEVICE_OBJECT *dev,
	FILE_OBJECT *file,
	LARGE_INTEGER *file_size)
{
	FILE_END_OF_FILE_INFORMATION end_of_file;
	end_of_file.EndOfFile.QuadPart = file_size->QuadPart;
	return cfFileSetInformation(
		dev,file,FileEndOfFileInformation,
		NULL,(void *)&end_of_file,
		sizeof(FILE_END_OF_FILE_INFORMATION));
}

NTSTATUS
cfFileGetStandInfo(
	PDEVICE_OBJECT dev,
	PFILE_OBJECT file,
	PLARGE_INTEGER allocate_size,
	PLARGE_INTEGER file_size,
	BOOLEAN *dir)
{
	NTSTATUS status;
	PFILE_STANDARD_INFORMATION infor = NULL;
	infor = (PFILE_STANDARD_INFORMATION)
		ExAllocatePoolWithTag(NonPagedPool,sizeof(FILE_STANDARD_INFORMATION),CF_MEM_TAG);
	if(infor == NULL)
		return STATUS_INSUFFICIENT_RESOURCES;
	status = cfFileQueryInformation(dev,file,
		FileStandardInformation,(void *)infor,
		sizeof(FILE_STANDARD_INFORMATION));
	if(NT_SUCCESS(status))
	{
		if(allocate_size != NULL)
			*allocate_size = infor->AllocationSize;
		if(file_size != NULL)
			*file_size = infor->EndOfFile;
		if(dir != NULL)
			*dir = infor->Directory;
	}
	ExFreePool(infor);
	return status;
}


NTSTATUS 
cfFileReadWrite( 
    DEVICE_OBJECT *dev, 
    FILE_OBJECT *file,
    LARGE_INTEGER *offset,
    ULONG *length,
    void *buffer,
    BOOLEAN read_write) 
{
	//ULONG i;
    PIRP irp;
    KEVENT event;
    PIO_STACK_LOCATION ioStackLocation;
	IO_STATUS_BLOCK IoStatusBlock = { 0 };

    KeInitializeEvent(&event, SynchronizationEvent, FALSE);

	// ����irp.
    irp = IoAllocateIrp(dev->StackSize, FALSE);
    if(irp == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
  
	// ��д���塣
    irp->AssociatedIrp.SystemBuffer = NULL;
	// ��paging io������£��ƺ�����Ҫʹ��MDL�����������С�����ʹ��UserBuffer.
	// �����Ҳ����϶���һ�㡣���������һ�����ԡ��Ա��ҿ��Ը��ٴ���
    irp->MdlAddress = NULL;
    irp->UserBuffer = buffer;
    irp->UserEvent = &event;
    irp->UserIosb = &IoStatusBlock;
    irp->Tail.Overlay.Thread = PsGetCurrentThread();
    irp->Tail.Overlay.OriginalFileObject = file;
    irp->RequestorMode = KernelMode;
	if(read_write)
		irp->Flags = IRP_DEFER_IO_COMPLETION|IRP_READ_OPERATION|IRP_NOCACHE;
	else
		irp->Flags = IRP_DEFER_IO_COMPLETION|IRP_WRITE_OPERATION|IRP_NOCACHE;

	// ��дirpsp
    ioStackLocation = IoGetNextIrpStackLocation(irp);
	if(read_write)
		ioStackLocation->MajorFunction = IRP_MJ_READ;
	else
		ioStackLocation->MajorFunction = IRP_MJ_WRITE;
    ioStackLocation->MinorFunction = IRP_MN_NORMAL;
    ioStackLocation->DeviceObject = dev;
    ioStackLocation->FileObject = file;
	if(read_write)
	{
		ioStackLocation->Parameters.Read.ByteOffset = *offset;
		ioStackLocation->Parameters.Read.Length = *length;
	}
	else
	{
		ioStackLocation->Parameters.Write.ByteOffset = *offset;
		ioStackLocation->Parameters.Write.Length = *length;
	}

	// �������
    IoSetCompletionRoutine(irp, cfFileIrpComp, 0, TRUE, TRUE, TRUE);
    (void) IoCallDriver(dev, irp);
    KeWaitForSingleObject(&event, Executive, KernelMode, TRUE, 0);
	*length = IoStatusBlock.Information;
    return IoStatusBlock.Status;
}

// ������
void cfFileCacheClear(PFILE_OBJECT pFileObject)
{
   PFSRTL_COMMON_FCB_HEADER pFcb;
   LARGE_INTEGER liInterval;
   BOOLEAN bNeedReleaseResource = FALSE;
   BOOLEAN bNeedReleasePagingIoResource = FALSE;
   KIRQL irql;

   pFcb = (PFSRTL_COMMON_FCB_HEADER)pFileObject->FsContext;
   if(pFcb == NULL)
       return;

   irql = KeGetCurrentIrql();
   if (irql >= DISPATCH_LEVEL)
   {
       return;
   }

   liInterval.QuadPart = -1 * (LONGLONG)50;

   while (TRUE)
   {
       BOOLEAN bBreak = TRUE;
       BOOLEAN bLockedResource = FALSE;
       BOOLEAN bLockedPagingIoResource = FALSE;
       bNeedReleaseResource = FALSE;
       bNeedReleasePagingIoResource = FALSE;

	   // ��fcb��ȥ������
       if (pFcb->PagingIoResource)
           bLockedPagingIoResource = ExIsResourceAcquiredExclusiveLite(pFcb->PagingIoResource);

	   // ��֮һ��Ҫ�õ��������
       if (pFcb->Resource)
       {
           bLockedResource = TRUE;
           if (ExIsResourceAcquiredExclusiveLite(pFcb->Resource) == FALSE)
           {
               bNeedReleaseResource = TRUE;
               if (bLockedPagingIoResource)
               {
                   if (ExAcquireResourceExclusiveLite(pFcb->Resource, FALSE) == FALSE)
                   {
                       bBreak = FALSE;
                       bNeedReleaseResource = FALSE;
                       bLockedResource = FALSE;
                   }
               }
               else
                   ExAcquireResourceExclusiveLite(pFcb->Resource, TRUE);
           }
       }
   
       if (bLockedPagingIoResource == FALSE)
       {
           if (pFcb->PagingIoResource)
           {
               bLockedPagingIoResource = TRUE;
               bNeedReleasePagingIoResource = TRUE;
               if (bLockedResource)
               {
                   if (ExAcquireResourceExclusiveLite(pFcb->PagingIoResource, FALSE) == FALSE)
                   {
                       bBreak = FALSE;
                       bLockedPagingIoResource = FALSE;
                       bNeedReleasePagingIoResource = FALSE;
                   }
               }
               else
               {
                   ExAcquireResourceExclusiveLite(pFcb->PagingIoResource, TRUE);
               }
           }
       }

       if (bBreak)
       {
           break;
       }
       
       if (bNeedReleasePagingIoResource)
       {
           ExReleaseResourceLite(pFcb->PagingIoResource);
       }
       if (bNeedReleaseResource)
       {
           ExReleaseResourceLite(pFcb->Resource);
       }

       if (irql == PASSIVE_LEVEL)
       {
           KeDelayExecutionThread(KernelMode, FALSE, &liInterval);
       }
       else
       {
           KEVENT waitEvent;
           KeInitializeEvent(&waitEvent, NotificationEvent, FALSE);
           KeWaitForSingleObject(&waitEvent, Executive, KernelMode, FALSE, &liInterval);
       }
   }

   if (pFileObject->SectionObjectPointer)
   {
		IO_STATUS_BLOCK ioStatus;
		CcFlushCache(pFileObject->SectionObjectPointer, NULL, 0, &ioStatus);
		if (pFileObject->SectionObjectPointer->ImageSectionObject)
		{
			MmFlushImageSection(pFileObject->SectionObjectPointer,MmFlushForWrite); // MmFlushForDelete
		}
		CcPurgeCacheSection(pFileObject->SectionObjectPointer, NULL, 0, FALSE);
   }

   if (bNeedReleasePagingIoResource)
   {
       ExReleaseResourceLite(pFcb->PagingIoResource);
   }
   if (bNeedReleaseResource)
   {
       ExReleaseResourceLite(pFcb->Resource);
   }
}

/////////////////////////////////////////cf_file_irp.c/////////////////////////////



//////////////////////////////////////////cf_create.c/////////////////////////////

// ��create֮ǰ��ʱ�򣬻��������·����
ULONG
cfFileFullPathPreCreate(
						PFILE_OBJECT file,
                        PUNICODE_STRING path
						)
{
	NTSTATUS status;
	POBJECT_NAME_INFORMATION  obj_name_info = NULL;
	WCHAR buf[64] = { 0 };
	void *obj_ptr;
	ULONG length = 0;
	BOOLEAN need_split = FALSE;

	ASSERT( file != NULL );
	if(file == NULL)
		return 0;
	if(file->FileName.Buffer == NULL)
		return 0;

	obj_name_info = (POBJECT_NAME_INFORMATION)buf;
	do {

		// ��ȡFileNameǰ��Ĳ��֣��豸·�����߸�Ŀ¼·����
		if(file->RelatedFileObject != NULL)
			obj_ptr = (void *)file->RelatedFileObject;
		else
			obj_ptr= (void *)file->DeviceObject;
		status = ObQueryNameString(obj_ptr,obj_name_info,64*sizeof(WCHAR),&length);
		if(status == STATUS_INFO_LENGTH_MISMATCH)
		{
			obj_name_info = ExAllocatePoolWithTag(NonPagedPool,length,CF_MEM_TAG);
			if(obj_name_info == NULL)
				return STATUS_INSUFFICIENT_RESOURCES;
			RtlZeroMemory(obj_name_info,length);
			status = ObQueryNameString(obj_ptr,obj_name_info,length,&length);            
		}
		// ʧ���˾�ֱ����������
		if(!NT_SUCCESS(status))
			break;

		// �ж϶���֮���Ƿ���Ҫ��һ��б�ܡ�����Ҫ��������:
		// FileName��һ���ַ�����б�ܡ�obj_name_info���һ��
		// �ַ�����б�ܡ�
		if( file->FileName.Length > 2 &&
			file->FileName.Buffer[ 0 ] != L'\\' &&
			obj_name_info->Name.Buffer[ obj_name_info->Name.Length / sizeof(WCHAR) - 1 ] != L'\\' )
			need_split = TRUE;

		// ���������ֵĳ��ȡ�������Ȳ��㣬Ҳֱ�ӷ��ء�
		length = obj_name_info->Name.Length + file->FileName.Length;
		if(need_split)
			length += sizeof(WCHAR);
		if(path->MaximumLength < length)
			break;

		// �Ȱ��豸��������ȥ��
		RtlCopyUnicodeString(path,&obj_name_info->Name);
		if(need_split)
			// ׷��һ��б��
			RtlAppendUnicodeToString(path,L"\\");

		// Ȼ��׷��FileName
		RtlAppendUnicodeStringToString(path,&file->FileName);
	} while(0);

	// ���������ռ���ͷŵ���
	if((void *)obj_name_info != (void *)buf)
		ExFreePool(obj_name_info);
	return length;
}

// ��IoCreateFileSpecifyDeviceObjectHint�����ļ���
// ����ļ���֮�󲻽�������������Կ���ֱ��
// Read��Write,���ᱻ���ܡ�
HANDLE cfCreateFileAccordingIrp(
   IN PDEVICE_OBJECT dev,
   IN PUNICODE_STRING file_full_path,
   IN PIO_STACK_LOCATION irpsp,
   OUT NTSTATUS *status,
   OUT PFILE_OBJECT *file,
   OUT PULONG information)
{
	HANDLE file_h = NULL;
	IO_STATUS_BLOCK io_status;
	ULONG desired_access;
	ULONG disposition;
	ULONG create_options;
	ULONG share_access;
	ULONG file_attri;
    OBJECT_ATTRIBUTES obj_attri;

    ASSERT(irpsp->MajorFunction == IRP_MJ_CREATE);

    *information = 0;

    // ��дobject attribute
    InitializeObjectAttributes(
        &obj_attri,
        file_full_path,
        OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,
        NULL,
        NULL);

    // ���IRP�еĲ�����
	desired_access = irpsp->Parameters.Create.SecurityContext->DesiredAccess;
	disposition = (irpsp->Parameters.Create.Options>>24);
	create_options = (irpsp->Parameters.Create.Options & 0x00ffffff);
	share_access = irpsp->Parameters.Create.ShareAccess;
	file_attri = irpsp->Parameters.Create.FileAttributes;

    // ����IoCreateFileSpecifyDeviceObjectHint���ļ���
    *status = IoCreateFileSpecifyDeviceObjectHint(
        &file_h,
        desired_access,
        &obj_attri,
        &io_status,
        NULL,
        file_attri,
        share_access,
        disposition,
        create_options,
        NULL,
        0,
        CreateFileTypeNone,
        NULL,
        0,
        dev);

    if(!NT_SUCCESS(*status))
        return file_h;

    // ��סinformation,��������ʹ�á�
    *information = io_status.Information;

    // �Ӿ���õ�һ��fileobject���ں���Ĳ������ǵ�һ��Ҫ���
    // ���á�
    *status = ObReferenceObjectByHandle(
        file_h,
        0,
        *IoFileObjectType,
        KernelMode,
        file,
        NULL);

    // ���ʧ���˾͹رգ�����û���ļ����������ʵ�����ǲ�
    // Ӧ�ó��ֵġ�
    if(!NT_SUCCESS(*status))
    {
        ASSERT(FALSE);
        ZwClose(file_h);
    }
    return file_h;
}

// д��һ���ļ�ͷ��
NTSTATUS cfWriteAHeader(PFILE_OBJECT file,PDEVICE_OBJECT next_dev)
{
    static WCHAR header_flags[CF_FILE_HEADER_SIZE/sizeof(WCHAR)] = {L'C',L'F',L'H',L'D'};
    LARGE_INTEGER file_size,offset;
    ULONG length = CF_FILE_HEADER_SIZE;
    NTSTATUS status;

    offset.QuadPart = 0;
    file_size.QuadPart = CF_FILE_HEADER_SIZE;
    // ���������ļ��Ĵ�СΪ4k��
    status = cfFileSetFileSize(next_dev,file,&file_size);
    if(status != STATUS_SUCCESS)
        return status;

    // Ȼ��д��8���ֽڵ�ͷ��
   return cfFileReadWrite(next_dev,file,&offset,&length,header_flags,FALSE);
}


// ��Ԥ����
ULONG cfIrpCreatePre(
    PIRP irp,
    PIO_STACK_LOCATION irpsp,
    PFILE_OBJECT file,
    PDEVICE_OBJECT next_dev)
{
    UNICODE_STRING path = { 0 };
    // ���Ȼ��Ҫ���ļ���·����
    ULONG length = cfFileFullPathPreCreate(file,&path);
    NTSTATUS status;
    ULONG ret = SF_IRP_PASS;
    PFILE_OBJECT my_file = NULL;
    HANDLE file_h;
    ULONG information = 0;
    LARGE_INTEGER file_size,offset = { 0 };
    BOOLEAN dir,sec_file;
    // ��ô򿪷���������
	ULONG desired_access = irpsp->Parameters.Create.SecurityContext->DesiredAccess;
    WCHAR header_flags[4] = {L'C',L'F',L'H',L'D'};
    WCHAR header_buf[4] = { 0 };
    ULONG disp;

    // �޷��õ�·����ֱ�ӷŹ����ɡ�
    if(length == 0)
        return SF_IRP_PASS;

    // ���ֻ�����Ŀ¼�Ļ���ֱ�ӷŹ�
    if(irpsp->Parameters.Create.Options & FILE_DIRECTORY_FILE)
        return SF_IRP_PASS;

    do {

        // ��path���仺����
        path.Buffer = ExAllocatePoolWithTag(NonPagedPool,length+4,CF_MEM_TAG);
        path.Length = 0;
        path.MaximumLength = (USHORT)length + 4;
        if(path.Buffer == NULL)
        {
            // �ڴ治�����������ֱ�ӹҵ�
            status = STATUS_INSUFFICIENT_RESOURCES;
            ret = SF_IRP_COMPLETED;
            break;
        }
        length = cfFileFullPathPreCreate(file,&path);

        // �õ���·����������ļ���
        file_h = cfCreateFileAccordingIrp(
            next_dev,
            &path,
            irpsp,
            &status,
            &my_file,
            &information);

        // ���û�гɹ��Ĵ򿪣���ô˵�����������Խ�����
        if(!NT_SUCCESS(status))
        {
            ret = SF_IRP_COMPLETED;
            break;
        }

        // �õ���my_file֮�������ж�����ļ��ǲ����Ѿ���
        // ���ܵ��ļ�֮�С�����ڣ�ֱ�ӷ���passthru����
        cfListLock();
        sec_file = cfIsFileCrypting(my_file);
        cfListUnlock();
        if(sec_file)
        {
            ret = SF_IRP_PASS;
            break;
        }

        // ������Ȼ�򿪣���������Ȼ������һ��Ŀ¼��������
        // �ж�һ�¡�ͬʱҲ���Եõ��ļ��Ĵ�С��
        status = cfFileGetStandInfo(
	        next_dev,
	        my_file,
	        NULL,
	        &file_size,
	        &dir);

        // ��ѯʧ�ܡ���ֹ�򿪡�
        if(!NT_SUCCESS(status))
        {
            ret = SF_IRP_COMPLETED;
            break;
        }

        // �������һ��Ŀ¼����ô�������ˡ�
        if(dir)
        {
            ret = SF_IRP_PASS;
            break;
        }

        // ����ļ���СΪ0������д�����׷�����ݵ���ͼ��
        // ��Ӧ�ü����ļ���Ӧ��������д���ļ�ͷ����Ҳ��Ψ
        // һ��Ҫд���ļ�ͷ�ĵط���
        if(file_size.QuadPart == 0 && 
            (desired_access & 
                (FILE_WRITE_DATA| 
		        FILE_APPEND_DATA)))
        {
            // �����Ƿ�ɹ���һ��Ҫд��ͷ��
            cfWriteAHeader(my_file,next_dev);
            // д��ͷ֮������ļ����ڱ�����ܵ��ļ�
            ret = SF_IRP_GO_ON;
            break;
        }

        // ����ļ��д�С�����Ҵ�СС��ͷ���ȡ�����Ҫ���ܡ�
        if(file_size.QuadPart < CF_FILE_HEADER_SIZE)
        {
            ret = SF_IRP_PASS;
            break;
        }

        // ���ڶ�ȡ�ļ����Ƚ������Ƿ���Ҫ���ܣ�ֱ�Ӷ���8��
        // �ھ��㹻�ˡ�����ļ��д�С�����ұ�CF_FILE_HEADER_SIZE
        // ������ʱ����ǰ8���ֽڣ��ж��Ƿ�Ҫ���ܡ�
        length = 8;
        status = cfFileReadWrite(next_dev,my_file,&offset,&length,header_buf,TRUE);
        if(status != STATUS_SUCCESS)
        {
            // ���ʧ���˾Ͳ������ˡ�
            ASSERT(FALSE);
            ret = SF_IRP_PASS;
            break;
        }
        // ��ȡ�����ݣ��ȽϺͼ��ܱ�־��һ�µģ����ܡ�
        if(RtlCompareMemory(header_flags,header_buf,8) == 8)
        {
            // ��������Ϊ�Ǳ�����ܵġ���������£����뷵��GO_ON.
            ret = SF_IRP_GO_ON;
            break;
        }

        // ������������ǲ���Ҫ���ܵġ�
        ret = SF_IRP_PASS;
    } while(0);

    if(path.Buffer != NULL)
        ExFreePool(path.Buffer);    
    if(file_h != NULL)
        ZwClose(file_h);
    if(ret == SF_IRP_GO_ON)
    {
        // Ҫ���ܵģ�������һ�»��塣�����ļ�ͷ�����ڻ����
        cfFileCacheClear(my_file);
    }
    if(my_file != NULL)
        ObDereferenceObject(my_file);

    // ���Ҫ������ɣ����������������ɡ���һ�㶼��
    // �Դ�����Ϊ��ֵġ�
    if(ret == SF_IRP_COMPLETED)
    {
		irp->IoStatus.Status = status;
		irp->IoStatus.Information = information;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
    }

    // Ҫע��:
    // 1.�ļ���CREATE��ΪOPEN.
    // 2.�ļ���OVERWRITEȥ���������ǲ���Ҫ���ܵ��ļ���
    // ������������������Ļ�����������ͼ�����ļ��ģ�
    // ��������ļ��Ѿ������ˡ�������ͼ�����ļ��ģ���
    // ����һ�λ�ȥ������ͷ��
    disp = FILE_OPEN;
    irpsp->Parameters.Create.Options &= 0x00ffffff;
    irpsp->Parameters.Create.Options |= (disp << 24);
    return ret;
}

/////////////////////////////////////////cf_create.c//////////////////////////////