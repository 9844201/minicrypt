//����ļ�  ����������е�Ӧ�ò�������������Ĺ��еĲ���  ʹӦ�ò���������ͨ�ű�֤ͳһ
// ���·������
#define MAXPATHLEN	1024

// ����¼��
#define MAXRULES	50

//Ҫ���ص��ļ�������󳤶�
#define    HIDERLEN   64


//����һ���֪��Ҫ��Ҫʹ�� 
#define HIDE_FLAG_FILE			1L//Ҫ���ص���ĳ���ļ�
#define HIDE_FLAG_DIRECTORY		2L//Ҫ���ص���ĳ��Ŀ¼

//ע��  ������������Ҫͳһ
//
//�豸��DOS NAME
//
#define DOS_DEVICE_NAME L"\\DosDevices\\LSFilter"


#define DEVICENAME _T("\\\\.\\LSFilter")



//��ȡlogbuf
#define IOCTL_GETLOGBUF \
CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)//��������ʽ

//���logbuf
#define IOCTL_ZEROLOGBUF \
CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)//��������ʽ

//������ر� ��ȡlogbuf
#define IOCTL_CONTROLLOG \
CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)//��������ʽ


// ��ʼ����ʱ���������н�ֹ���ʵ��ļ�
#define IOCTL_INIT_NOACCESSFILE \
CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)//��������ʽ

// ����������������һ����ֹ���ʵ��ļ�
#define IOCTL_ADD_NOACCESSFILE \
CTL_CODE(FILE_DEVICE_UNKNOWN, 0x808, METHOD_BUFFERED, FILE_ANY_ACCESS)//��������ʽ

// ������������ɾ��һ����ֹ���ʵ��ļ�
#define IOCTL_DEL_NOACCESSFILE \
CTL_CODE(FILE_DEVICE_UNKNOWN, 0x809, METHOD_BUFFERED, FILE_ANY_ACCESS)//��������ʽ



// ��ʼ����ʱ���������н�ֹɾ�����ļ�
#define IOCTL_INIT_NODELETEFILE \
CTL_CODE(FILE_DEVICE_UNKNOWN, 0x810, METHOD_BUFFERED, FILE_ANY_ACCESS)//��������ʽ

// ����������������һ����ֹɾ�����ļ�
#define IOCTL_ADD_NODELETEFILE \
CTL_CODE(FILE_DEVICE_UNKNOWN, 0x811, METHOD_BUFFERED, FILE_ANY_ACCESS)//��������ʽ

// ������������ɾ��һ����ֹɾ�����ļ�
#define IOCTL_DEL_NODELETEFILE \
CTL_CODE(FILE_DEVICE_UNKNOWN, 0x812, METHOD_BUFFERED, FILE_ANY_ACCESS)//��������ʽ



// ��ʼ����ʱ�������������ص��ļ�
#define IOCTL_INIT_HIDE \
CTL_CODE(FILE_DEVICE_UNKNOWN, 0x813, METHOD_BUFFERED, FILE_ANY_ACCESS)//��������ʽ

// ����������������һ�����ص��ļ�
#define IOCTL_ADD_HIDE \
CTL_CODE(FILE_DEVICE_UNKNOWN, 0x814, METHOD_BUFFERED, FILE_ANY_ACCESS)//��������ʽ

// ������������ɾ��һ�������ص��ļ�
#define IOCTL_DEL_HIDE \
CTL_CODE(FILE_DEVICE_UNKNOWN, 0x815, METHOD_BUFFERED, FILE_ANY_ACCESS)//��������ʽ

// ����������������һ���ض�����ļ�
#define IOCTL_ADD_REPARSE \
CTL_CODE(FILE_DEVICE_UNKNOWN, 0x816, METHOD_BUFFERED, FILE_ANY_ACCESS)//��������ʽ

// ������������ɾ��һ���ض�����ļ�
#define IOCTL_DEL_REPARSE \
CTL_CODE(FILE_DEVICE_UNKNOWN, 0x817, METHOD_BUFFERED, FILE_ANY_ACCESS)//��������ʽ

