//TOOL:DumpHeap.dll
//AUTHOR:David Mburu

#define INITGUID    /*To interpret IID interfaces as GUID*/
#define __field_ecount_opt(x) /*i am using gcc and this sal is not defined in gcc so i define it myself*/
#define KDEXT_64BIT /*To use 64bit pointers in my code*/
#define WIN32_LEAN_AND_MEAN /*mostly used to avoid function collission*/

/*Define the headers*/
#include <windows.h>
#include <stdio.h>
#include <dbgeng.h>
#include <wdbgexts.h>
/*GCC have no idea of __uuidof which is definetly define by default in MSVC*/
#ifdef __GNUC__
#define __uuidof(x)&IID_##x
#else
#define __uuidof(x)IID_##x
#endif

/*we define the heap offsets from the base:This offsets are specific to windows 7 x64bit*/
#define Size 0x8
#define Flags 0xa
#define PrevSize 0xc
#define BaseAddress 0x30
#define NumberOfPages 0x38
#define FirstEntry 0x40
#define Encoding 0x80
#define FrontEndHeapType 0x182
#define NumberOfUncommitedPages 0x50
/*define the flags*/
#define HEAP_ENTRY_FLAG_BUSY 0x01
#define HEAP_ENTRY_FLAG_SEGMENT 0x08

/*define a cleanup macro*/
#define SafeFree(x) (x->lpVtbl->Release(x))

/*global variables where we will store data such as the WINDBG_EXTENSION_APIS64 structure*/
WINDBG_EXTENSION_APIS64 ExtensionApi;
USHORT MajorVersion;
USHORT MinorVersion;
/*First function to be exported*/
void WindbgExtensionDllInit(PWINDBG_EXTENSION_APIS lpExtensionApi,USHORT Major,USHORT Minor){
	ExtensionApi=*lpExtensionApi;
	MajorVersion=Major;
	MinorVersion=Minor;
}

/*The second function:The first and second function are mandatory if using wdbgexts*/
EXT_API_VERSION ApiVersion={6,0,EXT_API_VERSION_NUMBER64,0};
LPEXT_API_VERSION ExtensionApiVersion(void){
	return &ApiVersion;
}

IDebugControl *control=0;
IDebugDataSpaces *data=0;

/*Function to initiliaze the interfaces*/
HRESULT CALLBACK init(PDEBUG_CLIENT client,PCSTR Args){
	HRESULT hr;
	hr=DebugCreate(__uuidof(IDebugClient),(void**)&client);//create clientobject
	if(FAILED(hr)){
		MessageBoxA(0,"DebugCreate() Failed",__FUNCTION__,MB_ICONERROR);
		return E_FAIL;
	}
	/*IDebugControl interface query:i pass lpVtbl(pointer to pointer table since gcc doesn't define it*/
	hr=client->lpVtbl->QueryInterface(client,__uuidof(IDebugControl),(void**)&control);
	if(FAILED(hr)){
		MessageBox(0,"Failed at QueryInterface(IDebugControl",__FUNCTION__,MB_ICONERROR);
		return E_FAIL;
	}
	/*IDebugDataSpaces interface defination*/
	hr=client->lpVtbl->QueryInterface(client,__uuidof(IDebugDataSpaces),(void**)&data);
	if(FAILED(hr)){
		MessageBoxA(0,"QueryInterface(&IDebugDataSpaces)",__FUNCTION__,MB_ICONERROR);
		return E_FAIL;
	}
	return S_OK;
}
void _stdcall dumpheap(HANDLE hCurrentProc,HANDLE hCurrentThread,ULONG CurrentPc,ULONG processor,PCSTR Args){
	UNREFERENCED_PARAMETER(hCurrentProc);
	UNREFERENCED_PARAMETER(hCurrentThread);
	UNREFERENCED_PARAMETER(CurrentPc);
	UNREFERENCED_PARAMETER(processor);
	if(!Args||!*Args){
		control->lpVtbl->Output(control,DEBUG_OUTPUT_ERROR,"Usage:!dumpheap <address>\n");
	}
	ULONG64 addr;
	sscanf_s(Args,"%llx",&addr);   /*get base address of heap*/
	control->lpVtbl->Output(control,DEBUG_OUTPUT_NORMAL,"Address:%llx\n",addr);
	USHORT size=0;
	USHORT prevsize=0;
	UCHAR flags=0;
	UCHAR frontendtype;
	ULONG NoPages=0;
	ULONG64 Key,Entry_Addr;
	ULONG64 encoding=addr+Encoding;
	HRESULT hr;

	/*read the FirstEntry address*/
	hr=data->lpVtbl->ReadVirtual(data,addr+FirstEntry,&Entry_Addr,sizeof(Entry_Addr),NULL);
	if(hr!=S_OK){
		control->lpVtbl->Output(control,DEBUG_OUTPUT_ERROR,"Failed to read FirstEntry:0x%X\n",hr);
	}

	/*read the number of pages*/
	hr=data->lpVtbl->ReadVirtual(data,addr+NumberOfPages,&NoPages,4,NULL);
	if(hr!=S_OK){
		control->lpVtbl->Output(control,DEBUG_OUTPUT_ERROR,"Failed to read No. of pages:0x%X\n",hr);
	}
	/* read the Encoding key*/
	hr=data->lpVtbl->ReadVirtual(data,encoding+8,&Key,sizeof(Key),NULL);
	if(hr!=S_OK){
		control->lpVtbl->Output(control,DEBUG_OUTPUT_ERROR,"Failed to read Encoding:0x%x\n",hr);
	}


	control->lpVtbl->Output(control,DEBUG_OUTPUT_ERROR,"Walking the heap @ 0x%016x\n",addr);
	/*read the no.of uncommited pages*/
	ULONG uncommit_pages;
	hr=data->lpVtbl->ReadVirtual(data,addr+NumberOfUncommitedPages,&uncommit_pages,sizeof(uncommit_pages),NULL);
	if(hr!=S_OK){
		control->lpVtbl->Output(control,DEBUG_OUTPUT_NORMAL,"Failed to read uncommited pages\n");
	}
	ULONG regionsize=NoPages*0x1000;
	ULONG Commit_pages=regionsize-((uncommit_pages)*0x1000);/*get the number of commited pages by subtracting No. of Uncommited pages from No. of Pages and multiply by 0x1000 which is one page size to get the size of the commited area*/
	ULONG64 HeapEnd=addr+Commit_pages;
	control->lpVtbl->Output(control,DEBUG_OUTPUT_ERROR,"RegionSize:0x%X From 0x%016x To 0x%016x\n",Commit_pages,addr,HeapEnd);
	control->lpVtbl->Output(control,DEBUG_OUTPUT_ERROR,"Address                   Size            Flags        State   FrontEndType   PrevSize\n");
	control->lpVtbl->Output(control,DEBUG_OUTPUT_ERROR,"--------------------------------------------------------------------------------------------------------------\n");
	ULONG64 current=Entry_Addr;
	while(current<HeapEnd){
		/*read the encoded heap header*/
		ULONG64 header;
		hr=data->lpVtbl->ReadVirtual(data,current+8,&header,sizeof(header),NULL);
		if(FAILED(hr)){
			control->lpVtbl->Output(control,DEBUG_OUTPUT_NORMAL,"Failed to read header:0x%x\n",hr);
		}
		/*some of heap metadata fields are encoded by xoring the fields with the key in Encoding Field in HEAP structure so we decode it*/
		ULONG64 RawHeader=header^Key;/*xor the whole field of HEAP_ENTRY*/
		size=(USHORT)RawHeader&0xFFFF;
		flags=(UCHAR)((RawHeader>>16)&0xFF);
		prevsize=(USHORT)((RawHeader>>32)&0xFF);
		/*read the frontendheaptype*/
		hr=data->lpVtbl->ReadVirtual(data,addr+FrontEndHeapType,&frontendtype,2,NULL);
		if(FAILED(hr)){
			control->lpVtbl->Output(control,DEBUG_OUTPUT_ERROR,"Failed to read Frontendheaptype:0x%x\n",hr);
			break;
		}
		const char* state=(flags&HEAP_ENTRY_FLAG_BUSY)?"BUSY":"FREE";
		const char* HeapType=(frontendtype==0)?"NO LFH":(frontendtype==1)?"LookAsideList":(frontendtype==2)?"LFH enabled":"VS segment";
		const char* seg=(flags&HEAP_ENTRY_FLAG_SEGMENT)?"Seg":"";

		control->lpVtbl->Output(control,DEBUG_OUTPUT_NORMAL,"0x%016x        0x%04x0        0x%02x0        %s        %s        0x%04x0        %s\n",current,size,flags,state,HeapType,prevsize,seg);
		ULONG64 chunksize=size*16;
		if(chunksize==0)break;
		current+=chunksize;
	}
	control->lpVtbl->Output(control,DEBUG_OUTPUT_NORMAL,"Finished walking the heap...\n");
	SafeFree(control);
}


















































