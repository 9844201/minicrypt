

/////////////////////////////////////////////////////////////////////////////
//
//                  �ļ����� �Լ���غ�����ʵ��
//
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////
//****************************************************************************************************
//�ļ�������Ҫ���ӵ�ȫ�ֱ���
LIST_ENTRY		g_HideObjHead;			//�����б�

//�ļ�������Ҫ���ӵĺ���

/*
*	�����Ƿ���Ҫ���صĶ���
*/
BOOLEAN
IS_MY_HIDE_OBJECT(const WCHAR *Name, ULONG NameLenth, ULONG Flag,PHIDE_DIRECTOR temHideDir)
{
	PLIST_ENTRY headListEntry = &(temHideDir->link);
	PLIST_ENTRY tmpListEntry = headListEntry;
	PHIDE_FILE tmpHideFile = NULL;
	ULONG ObjFlag = (FILE_ATTRIBUTE_DIRECTORY & Flag)?HIDE_FLAG_DIRECTORY:HIDE_FLAG_FILE;
	
	if (IsListEmpty(headListEntry))
	{
		return FALSE;
	}
	while (tmpListEntry->Flink != headListEntry)
	{
		tmpListEntry = tmpListEntry->Flink;
		tmpHideFile = (PHIDE_FILE)CONTAINING_RECORD(tmpListEntry, HIDE_FILE, linkfield);
		if ((ObjFlag == tmpHideFile->Flag) &&
			(0 == wcsncmp(Name, tmpHideFile->Name, NameLenth>>1)))
		{
			KdPrint(("Find Obj@=@=@=@=@=@=@=@=@=@&&&%%%^^^***###\n"));
			return TRUE;
		}
	}
	return FALSE;
}
VOID
AddHideObject(PHider addHide)
{
	//���һ������
	PHIDE_FILE newHideObj;
	PHIDE_DIRECTOR newHideDir;
	PLIST_ENTRY headListEntry = &g_HideObjHead;
	PLIST_ENTRY tmpListEntry = headListEntry;
	
	PHIDE_FILE tmpHideFile = NULL;
	PHIDE_DIRECTOR temHideDir = NULL;
	
	PWCHAR fatherPath=addHide->fatherpath;
	PWCHAR fileName=addHide->filename;
	PWCHAR hidAllPath=addHide->hiddenallpath;

    ULONG Flag;

	if (addHide->flag==HIDE_FLAG_DIRECTORY)
	{
		Flag=HIDE_FLAG_DIRECTORY;
	} 
	else
	{
		Flag=HIDE_FLAG_FILE;
	}
	
	//Ϊ�����ӵ�����·�������ڴ�
	newHideObj = ExAllocatePoolWithTag(PagedPool, sizeof(HIDE_FILE), 'NHFO');
	newHideDir = ExAllocatePoolWithTag(PagedPool, sizeof(HIDE_DIRECTOR), 'NHFO');
	
	InitializeListHead(&(newHideDir->link));
	
	//��ȡ���־λ
	newHideObj->Flag = Flag;
	
	//�������Ƶ����ǵ����ݽṹ��
	wcscpy(newHideDir->fatherPath, fatherPath);
	wcscpy(newHideObj->Name, fileName);
	
	//listentry����һ����Ա
	//InsertTailList(&g_HideObjHead, &newHideObj->linkfield);
	
	//�������ļ���ȫ·�����뵽�б���
	SfAddComPathHidAce(hidAllPath);
	
	//����б�ĸ�Ŀ¼Ϊ�գ���˵���б���û�У�ֱ�Ӳ��������
	if (IsListEmpty(&g_HideObjHead))
	{
		InsertTailList(&g_HideObjHead, &newHideDir->linkfield);
		InsertTailList(&(newHideDir->link), &newHideObj->linkfield);
		return;
	}
	
	//�������ԭ�������Ѿ���Ԫ���ˣ�Ӧ�ñ������ң��ٲ���
    while (tmpListEntry->Flink != headListEntry)//����������Ŀ¼�б�
	{
		//�������б�ȡ���������е�ֵ
		tmpListEntry = tmpListEntry->Flink;
		temHideDir = (PHIDE_DIRECTOR)CONTAINING_RECORD(tmpListEntry, HIDE_DIRECTOR, linkfield);
		tmpHideFile = (PHIDE_FILE)CONTAINING_RECORD((temHideDir->link.Flink), HIDE_FILE, linkfield);
		
        if (!wcscmp(temHideDir->fatherPath,fatherPath))
		{
			//����һ����ӦΪӦ�ò��Ѿ��жϣ���Ӳ��������ظ�·�������ԾͲ����ж��ˣ�
			//ֱ����������ڵ㴦����
			InsertTailList(&(temHideDir->link), &newHideObj->linkfield);
			return;
		}
	}
	//����л����ѭ������˵����������û���ҵ������ں������
	InsertTailList(&g_HideObjHead, &newHideDir->linkfield);
	InsertTailList(&(newHideDir->link), &newHideObj->linkfield);
	return;
}

BOOLEAN
HandleDirectory(IN OUT PFILE_BOTH_DIR_INFORMATION DirInfo, //�ļ�Ŀ¼����Ϣ
		IN PULONG lpBufLenth,PHIDE_DIRECTOR temHideDir)//����
{
	//����Ŀ¼����
	PFILE_BOTH_DIR_INFORMATION currentDirInfo = DirInfo;
	PFILE_BOTH_DIR_INFORMATION lastDirInfo = NULL;
	ULONG offset = 0;
	ULONG position = 0;
	ULONG newLenth = *lpBufLenth;
	//	WCHAR fileName[] = L"Test.txt";
	do
	{
		offset = currentDirInfo->NextEntryOffset;//�õ���һ����ƫ��  Ҳ�������Ŀ¼�е���һ���ļ���ַ
		//if (!(FILE_ATTRIBUTE_DIRECTORY & currentDirInfo->FileAttributes) &&
		//	 (0 == wcsncmp(currentDirInfo->FileName, fileName, currentDirInfo->FileNameLength>>1)))
		//�鿴�Ƿ�Ϊ���ǵ����ض���
		if (IS_MY_HIDE_OBJECT(currentDirInfo->FileName,//�ļ��� 
			currentDirInfo->FileNameLength,//�ļ����ĳ���
			currentDirInfo->FileAttributes,temHideDir))//�ļ�����
		{
			if (0 == offset)//û���������ļ�������
			{
				if (lastDirInfo)//��lastDirInfo��Ϊ��
				{
					lastDirInfo->NextEntryOffset = 0;//lastDirInfoָ����ļ�ƫ����ΪΪ0
					newLenth -= *lpBufLenth - position;//�µĳ��ȼ���
				}
				else
				{
					currentDirInfo->NextEntryOffset = 0;//currentDirInfoָ����ļ�ƫ����ΪΪ0
					*lpBufLenth = 0;//�³���Ϊ0
					return TRUE;//��������
				}
			}
			else//���ǻ�����һ���ļ�
			{
				//KdPrint(("n[%d][%d][%d]\n", newLenth, *lpBufLenth, position));
				RtlMoveMemory(currentDirInfo, (PUCHAR)currentDirInfo + offset, *lpBufLenth - position - offset);
				newLenth -= offset;
				position += offset;
			}
		}
		else//�����ǾͷŹ���  �鿴��һ������
		{
			position += offset;
			lastDirInfo = currentDirInfo;
			currentDirInfo = (PFILE_BOTH_DIR_INFORMATION)((PUCHAR)currentDirInfo + offset);
		}
	} while (0 != offset);
	*lpBufLenth = newLenth;
	return TRUE;
}

VOID
DelHideObject(PHider delHide)
{
	PLIST_ENTRY headListEntry = &g_HideObjHead;
	PLIST_ENTRY tmpListEntry = headListEntry;
	
	PLIST_ENTRY HideList = NULL;
	PLIST_ENTRY tmpHideList = NULL;
	
	PHIDE_FILE tmpHideFile = NULL;
	PHIDE_DIRECTOR temHideDir = NULL;
    PHIDE_FILE compareHideFile = NULL;
	
	//��Ӧ�ò㴫�����Ľṹ�������
	PWCHAR fatherPath=delHide->fatherpath;
	PWCHAR fileName=delHide->filename;
	PWCHAR hidAllPath=delHide->hiddenallpath;
    ULONG Flag;
	if (delHide->flag==HIDE_FLAG_DIRECTORY)
	{
		Flag=HIDE_FLAG_DIRECTORY;
	} 
	else
	{
		Flag=HIDE_FLAG_FILE;
	}
	SfDeleteComPathHidAce(hidAllPath);
	//����б�ĸ�Ŀ¼Ϊ�գ���˵���б���û�У�ֱ�ӷ���
	if (IsListEmpty(&g_HideObjHead))
	{
		return;
	}
	
	//�������ԭ�������Ѿ���Ԫ���ˣ�Ӧ�ñ�������
    while (tmpListEntry->Flink != headListEntry)//����������Ŀ¼�б�
	{
		//�������б�ȡ���������е�ֵ
		tmpListEntry = tmpListEntry->Flink;
		temHideDir = (PHIDE_DIRECTOR)CONTAINING_RECORD(tmpListEntry, HIDE_DIRECTOR, linkfield);
		tmpHideFile = (PHIDE_FILE)CONTAINING_RECORD((temHideDir->link.Flink), HIDE_FILE, linkfield);
		
		//����ҵ���Ŀ¼��ͬ��Ӧ�ñ���������Ŀ¼�����Ƿ����Ҫɾ���Ľڵ�
        if (!wcscmp(temHideDir->fatherPath,fatherPath))
		{
			HideList=&(temHideDir->link);
			tmpHideList=HideList;
			
			while (tmpHideList->Flink != HideList)
			{
				tmpHideList = tmpHideList->Flink;
				compareHideFile = (PHIDE_FILE)CONTAINING_RECORD(tmpHideList, HIDE_FILE, linkfield);
				if ((Flag == compareHideFile->Flag) &&
					(0 == wcsncmp(fileName, compareHideFile->Name, sizeof(fileName))))
				{
					//�ҵ��ˣ�ɾ����
					RemoveEntryList((PLIST_ENTRY)compareHideFile);
					ExFreePool(compareHideFile);
					if (IsListEmpty(HideList))
					{
						//���������Ϊ���ǣ���ɾ������
						RemoveEntryList((PLIST_ENTRY)temHideDir);
						ExFreePool(temHideDir);
					}
					return;
				}
			}
		}
	}
	//����л����ѭ������˵����������û���ҵ����򷵻�
	return;
}
