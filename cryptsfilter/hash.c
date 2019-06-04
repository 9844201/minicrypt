


/////////////////////////////////////////////////////////////////////////////
//
//                  hashtable�Լ���غ�����ʵ��
//
/////////////////////////////////////////////////////////////////////////////

typedef struct _nameentry {
	PFILE_OBJECT   FileObject;
	struct _nameentry *Next;
	WCHAR   FullPathName[];
} HASH_ENTRY, *PHASH_ENTRY;

#define NUMHASH   0x100//�����256��Ԫ��

ERESOURCE	        HashResource;

//����Ϊ��ϣ����
#if defined(_IA64_) 
#define HASHOBJECT(_fileobject)   (((ULONG_PTR)_fileobject)>>5)%NUMHASH
#else
#define HASHOBJECT(_fileobject)   (((ULONG)_fileobject)>>5)%NUMHASH
#endif

PHASH_ENTRY         HashTable[NUMHASH];//ȫ�ֱ���  ��ϣ����


//��غ���
VOID
SfHashCleanup(//��ж��ʱ �ͷ��ڴ�
	      VOID
	      )
{
	PHASH_ENTRY   hashEntry, nextEntry;
	ULONG      i;
	
	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &HashResource, TRUE );
	
	//
	// Free the hash table entries
	//
	for( i = 0; i < NUMHASH; i++ ) {
		
		hashEntry = HashTable[i];
		
		while( hashEntry ) {
			nextEntry = hashEntry->Next;
			ExFreePool( hashEntry );
			hashEntry = nextEntry;
		}
		
		HashTable[i] = NULL;
	}
	ExReleaseResourceLite( &HashResource );
	KeLeaveCriticalRegion();
//	KdPrint(("SfHashCleanup over"));
}

VOID 
SfFreeHashEntry( 
		PFILE_OBJECT fileObject 
		)
{
	PHASH_ENTRY   hashEntry, prevEntry;
	
	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &HashResource, TRUE );
	
	//��Ѱ�������
	hashEntry = HashTable[ HASHOBJECT( fileObject ) ];
	prevEntry = NULL;
	
	while( hashEntry && hashEntry->FileObject != fileObject ) {
		
		prevEntry = hashEntry;
		hashEntry = hashEntry->Next;
	}
	
	// 
	// If we fall of the hash list without finding what we're looking
	// for, just return.
	//
	if( !hashEntry ) {
		//     KdPrint(("fall to  find fileobject  to cleanup\n"));
		ExReleaseResourceLite( &HashResource );
		KeLeaveCriticalRegion();
		return;
	}
	
	//
	// Got it! Remove it from the list
	//    KdPrint(("Got it! find fileobject  to cleanup %s   %x\n",hashEntry->FullPathName,hashEntry->FileObject));
	if( prevEntry ) {
		
		prevEntry->Next = hashEntry->Next;
		
	} else {
		
		HashTable[ HASHOBJECT( fileObject )] = hashEntry->Next;
	}
	
	//
	// Free the entry's memory
	//
	ExFreePool( hashEntry );
	
	ExReleaseResourceLite( &HashResource );
	KeLeaveCriticalRegion();
}

BOOLEAN
SfGetFullPath( 
	      IN PFILE_OBJECT fileObject, 
	      IN OUT PWSTR fullPathName 
	      )
{
	PHASH_ENTRY         hashEntry;
	
	//
	// Lookup the object in the hash table to see if a name 
	// has already been generated for it
	//
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite( &HashResource, TRUE );
	
	hashEntry = HashTable[ HASHOBJECT( fileObject ) ];
	
	while( hashEntry && hashEntry->FileObject != fileObject ) {
		
		hashEntry = hashEntry->Next;
	}
	
	//
	// Did we find an entry?
	//
	if( hashEntry ) {
		
		//
		// Yes, so get the name from the entry.
		//
		KdPrint(( "in SfGetFullPath  %ws %x\n",hashEntry->FullPathName,hashEntry->FileObject));

		if (fullPathName==NULL)
		{
			return  TRUE;
		}
		wcscpy( fullPathName, hashEntry->FullPathName );
		ExReleaseResourceLite( &HashResource );
		KeLeaveCriticalRegion();
		return  TRUE;
	}
	
	ExReleaseResourceLite( &HashResource );
	KeLeaveCriticalRegion();
	return FALSE;
}


// VOID
// SfAddIntoHashTable(
// 		   PFILE_OBJECT fileObject, 
// 		   PANSI_STRING Ansiname
// 		   )
// {
// 	PHASH_ENTRY         newEntry;
// 	newEntry = ExAllocatePool( NonPagedPool, 
// 		sizeof(HASH_ENTRY ) + Ansiname->Length+ 1);
// 	
// 	//
// 	// If no memory for a new entry, oh well.
// 	//
// 	if( newEntry ) {
// 		ULONG  len=Ansiname->Length;
// 		ULONG  i;
// 		//
// 		// Fill in the new entry 
// 		//
// 		newEntry->FileObject = fileObject;
// 		//���� ANSISTRING �����������ݵ��ҵ�CHAR������ ������β����Ϊ'\0'
// 		for (i=0;i<len;i++)
// 		{
// 			newEntry->FullPathName[i]=*(Ansiname->Buffer+i);
// 		}
// 		newEntry->FullPathName[len]='\0' ;//0x00 NULL under free  always error
// 		KdPrint(("HashADD %s %x\n",newEntry->FullPathName,newEntry->FileObject));
// 		//
// 		// Put it in the hash table
// 		//
// 		KeEnterCriticalRegion();
// 		ExAcquireResourceExclusiveLite( &HashResource, TRUE );
// 		
// 		newEntry->Next = HashTable[ HASHOBJECT(fileObject) ];
// 		HashTable[ HASHOBJECT(fileObject) ] = newEntry; 
// 		
// 		ExReleaseResourceLite( &HashResource );
// 		KeLeaveCriticalRegion();
// 	}
// }

//��д��hashadd����   ��Ҫ�����ļ�����ԭʼ·���� ���豸��չ�е��豸����������
VOID
SfAddIntoHashTable(
				   PFILE_OBJECT fileObject, 
				   PUNICODE_STRING name,
				   PUNICODE_STRING devname,
				   WCHAR DriveLetter
				   )
{
	PHASH_ENTRY         newEntry;	
	PWSTR pname=name->Buffer;
	USHORT namelen=(name->Length)/2;
	USHORT devnamelen=(devname->Length)/2;
	USHORT i;
	newEntry =
	ExAllocatePoolWithTag( NonPagedPool, sizeof(HASH_ENTRY )+sizeof(WCHAR)*(namelen-devnamelen+3),'hash' );

	if(newEntry)
	{	
		RtlZeroMemory(newEntry,sizeof(HASH_ENTRY )+sizeof(WCHAR)*(namelen-devnamelen+3));      
		newEntry->FullPathName[0]=DriveLetter;
		newEntry->FullPathName[1]=':';
		for (i=0;i<(namelen-devnamelen);i++)
		{
			newEntry->FullPathName[2+i]=pname[devnamelen+i];
		}
		newEntry->FullPathName[namelen-devnamelen+2]='\0';
		newEntry->FileObject = fileObject;
			KdPrint(("HashADD %ws %x\n",newEntry->FullPathName,newEntry->FileObject));
		//
		// Put it in the hash table
		//
		KeEnterCriticalRegion();
		ExAcquireResourceExclusiveLite( &HashResource, TRUE );
		
		newEntry->Next = HashTable[ HASHOBJECT(fileObject) ];
		HashTable[ HASHOBJECT(fileObject) ] = newEntry; 
		
		ExReleaseResourceLite( &HashResource );
		KeLeaveCriticalRegion();
	}
}