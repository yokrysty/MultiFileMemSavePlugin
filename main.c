#include <phdk.h>

#define ID_SAVE_MULTI_MEM 222222

VOID MenuItemCallback(
    __in_opt PVOID Parameter,
    __in_opt PVOID Context
);

VOID GeneralCallbackMemoryMenuInitializingCallback(
    __in_opt PVOID Parameter,
    __in_opt PVOID Context
);

PPH_PLUGIN PluginInstance;
PH_CALLBACK_REGISTRATION PluginMenuItemCallbackRegistration;
PH_CALLBACK_REGISTRATION GeneralCallbackMemoryMenuInitializingRegistration;

LOGICAL DllMain(
    __in HINSTANCE Instance,
    __in ULONG Reason,
    __reserved PVOID Reserved
)
{
    switch (Reason)
    {
    case DLL_PROCESS_ATTACH:
        {
            PPH_PLUGIN_INFORMATION info;

            // Register your plugin with a unique name, otherwise it will fail.
            PluginInstance = PhRegisterPlugin(L"krysty.MultiFileMemSave", Instance, &info);

            if (!PluginInstance)
                return FALSE;

            info->DisplayName = L"MultiFileMemSave";
            info->Author = L"krysty";
            info->Description = L"Save multiple memory items in separate files.";
            info->HasOptions = FALSE;

            PhRegisterCallback(
                PhGetPluginCallback(PluginInstance, PluginCallbackMenuItem),
                MenuItemCallback,
                NULL,
                &PluginMenuItemCallbackRegistration
                );

            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackMemoryMenuInitializing),
                GeneralCallbackMemoryMenuInitializingCallback,
                NULL,
                &GeneralCallbackMemoryMenuInitializingRegistration
            );
        }
        break;
    }

    return TRUE;
}

VOID MenuItemCallback(
    __in_opt PVOID Parameter,
    __in_opt PVOID Context
    )
{
    PPH_PLUGIN_MENU_ITEM menuItem = Parameter;

    if (menuItem->Id != ID_SAVE_MULTI_MEM)
    {
        return;
    }

    const PPH_PLUGIN_MENU_INFORMATION menuInfo = menuItem->Context;
	
    static PH_FILETYPE_FILTER filters[] =
    {
	    { L"Binary files (*.bin)", L"*.bin" },
	    { L"All files (*.*)", L"*.*" }
    };

    PVOID fileDialog = PhCreateSaveFileDialog();
    PhSetFileDialogFilter(fileDialog, filters, sizeof filters / sizeof(PH_FILETYPE_FILTER));
    PhSetFileDialogFileName(fileDialog, L"Filename is not considered");
    if (!PhShowFileDialog(PhMainWndHandle, fileDialog))
    {
        return;
    }

    PPH_STRING fileName = PH_AUTO(PhGetFileDialogFileName(fileDialog));
    PH_STRINGREF pathPart;
    PH_STRINGREF baseNamePart;
    PhSplitStringRefAtLastChar(&fileName->sr, '\\', &pathPart, &baseNamePart);
    PPH_STRING basePath = PH_AUTO(PhCreateString2(&pathPart));

    PhFreeFileDialog(fileDialog);
	
    HANDLE processHandle;
    if (!(NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_VM_READ, menuInfo->u.Memory.ProcessId))))
    {
        PhShowError(PhMainWndHandle, L"Unable to open the process");
        return;
    }

    for (ULONG i = 0; i < menuInfo->u.Memory.NumberOfMemoryNodes; i++)
    {
        PPH_MEMORY_NODE memoryNode = menuInfo->u.Memory.MemoryNodes[i];
        PPH_MEMORY_ITEM memoryItem = memoryNode->MemoryItem;

        if (!memoryNode->IsAllocationBase && !(memoryItem->State & MEM_COMMIT))
            continue;

        fileName = PH_AUTO(PhFormatString(L"0x%x.bin", memoryItem->BaseAddress));
        PPH_STRING filePath = PH_AUTO(PhaConcatStrings(3, basePath->Buffer, L"\\", fileName->Buffer));

        PPH_FILE_STREAM fileStream;
        PVOID buffer;
        if (NT_SUCCESS(PhCreateFileStream(&fileStream, filePath->Buffer, FILE_GENERIC_WRITE, FILE_SHARE_READ, FILE_OVERWRITE_IF, 0)))
        {
            buffer = PhAllocatePage(PAGE_SIZE, NULL);
        	
            for (SIZE_T offset = 0; offset < memoryItem->RegionSize; offset += PAGE_SIZE)
            {
                if (NT_SUCCESS(NtReadVirtualMemory(
                    processHandle,
                    PTR_ADD_OFFSET(memoryItem->BaseAddress, offset),
                    buffer,
                    PAGE_SIZE,
                    NULL
                )))
                {
                    PhWriteFileStream(fileStream, buffer, PAGE_SIZE);
                }
            }

            PhFreePage(buffer);
            PhDereferenceObject(fileStream);
        }
    }
	
    NtClose(processHandle);

    PhShowInformation(PhMainWndHandle, L"Done");
}

VOID GeneralCallbackMemoryMenuInitializingCallback(
    __in_opt PVOID Parameter,
    __in_opt PVOID Context
)
{
	const PPH_PLUGIN_MENU_INFORMATION menuInfo = Parameter;
    	
    PhInsertEMenuItem(menuInfo->Menu, PhPluginCreateEMenuItem(PluginInstance, PH_EMENU_SEPARATOR, 0, NULL, 0), -1u);

    PPH_EMENU_ITEM menuItem = PhPluginCreateEMenuItem(PluginInstance, 0, ID_SAVE_MULTI_MEM, L"&Save Selected....", menuInfo);
    PhInsertEMenuItem(menuInfo->Menu, menuItem, -1u);
	
    if (menuInfo->u.Memory.NumberOfMemoryNodes > 0)
    {
        menuItem->Flags &= ~PH_EMENU_DISABLED;
    }
    else
    {
        menuItem->Flags |= PH_EMENU_DISABLED;
    }
}
