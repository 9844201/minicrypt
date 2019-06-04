
#define SF_IRP_GO_ON		    3L
#define SF_IRP_COMPLETED		4L
#define SF_IRP_PASS		        5L

////////////////////////////////////////////cf_proc.h/////////////////////////

void cfCurProcNameInit();

// ���º������Ի�ý����������ػ�õĳ��ȡ�
ULONG cfCurProcName(PUNICODE_STRING name);

// �жϵ�ǰ�����ǲ���notepad.exe
BOOLEAN cfIsCurProcSec(void);


//////////////////////////////////////////cf_proc.h////////////////////////////


/////////////////////////////////////////cf_modify_irp.h//////////////////////

void cfIrpSetInforPre(
					  PIRP irp,
					  PIO_STACK_LOCATION irpsp);

void cfIrpQueryInforPost(PIRP irp,PIO_STACK_LOCATION irpsp);

void cfIrpDirectoryControlPost(PIRP irp,PIO_STACK_LOCATION irpsp);

void cfIrpReadPre(PIRP irp,PIO_STACK_LOCATION irpsp);

void cfIrpReadPost(PIRP irp,PIO_STACK_LOCATION irpsp);

BOOLEAN cfIrpWritePre(PIRP irp,PIO_STACK_LOCATION irpsp,void **context);

void cfIrpWritePost(PIRP irp,PIO_STACK_LOCATION irpsp,void *context);


//////////////////////////////////////cf_modify_irp.h////////////////////////////

/////////////////////////////////////cf_list.h//////////////////////////////////

void cfListInit();
BOOLEAN cfListInited();
void cfListLock();
void cfListUnlock();
// �������һ���ļ����ж��Ƿ��ڼ��������С�
BOOLEAN cfIsFileCrypting(PFILE_OBJECT file);
BOOLEAN cfFileCryptAppendLk(PFILE_OBJECT file);
BOOLEAN cfIsFileNeedCrypt(
						  PFILE_OBJECT file,
						  PDEVICE_OBJECT next_dev,
						  ULONG desired_access,
						  BOOLEAN *need_write_header);
// �����ļ���clean up��ʱ����ô˺����������鷢��
// FileObject->FsContext���б���
BOOLEAN cfCryptFileCleanupComplete(PFILE_OBJECT file);
NTSTATUS cfWriteAHeader(PFILE_OBJECT file,PDEVICE_OBJECT next_dev);


/////////////////////////////////////cf_list.h////////////////////////////////////////


////////////////////////////////////cf_file_irp.h////////////////////////////////////

// �Է���SetInformation����.
NTSTATUS 
cfFileSetInformation( 
					 DEVICE_OBJECT *dev, 
					 FILE_OBJECT *file,
					 FILE_INFORMATION_CLASS infor_class,
					 FILE_OBJECT *set_file,
					 void* buf,
					 ULONG buf_len);

NTSTATUS
cfFileQueryInformation(
					   DEVICE_OBJECT *dev, 
					   FILE_OBJECT *file,
					   FILE_INFORMATION_CLASS infor_class,
					   void* buf,
					   ULONG buf_len);

NTSTATUS 
cfFileReadWrite( 
				DEVICE_OBJECT *dev, 
				FILE_OBJECT *file,
				LARGE_INTEGER *offset,
				ULONG *length,
				void *buffer,
				BOOLEAN read_write);

NTSTATUS
cfFileGetStandInfo(
				   PDEVICE_OBJECT dev,
				   PFILE_OBJECT file,
				   PLARGE_INTEGER allocate_size,
				   PLARGE_INTEGER file_size,
				   BOOLEAN *dir);

NTSTATUS
cfFileSetFileSize(
				  DEVICE_OBJECT *dev,
				  FILE_OBJECT *file,
				  LARGE_INTEGER *file_size);

// ������
void cfFileCacheClear(PFILE_OBJECT pFileObject);

//////////////////////////////////////////////cf_file_irp.h////////////////////////////


/////////////////////////////////////////////cf_create.h///////////////////////////////

// ��Ԥ������ע�⣬ֻ�е�ǰ����Ϊ���ܽ��̣�����Ҫ��
// �����Ԥ����������
ULONG cfIrpCreatePre(
					 PIRP irp,
					 PIO_STACK_LOCATION irpsp,
					 PFILE_OBJECT file,
					 PDEVICE_OBJECT next_dev);

/////////////////////////////////////////cf_create.h/////////////////////////////////
