DumpHeap — WinDbg Heap Inspection Extension
A research-oriented WinDbg extension written in C for inspecting NT heap metadata on Windows 7 x64. The project was built from the ground up using GCC to understand WinDbg extension APIs, debugger COM interfaces, vtables, and debugger memory-access mechanisms.


The code/tool that i will start with is a windbg extension that i coded myself in c.I actually compiled the code using the gcc to learn how windbg extensions works internally since gcc don't have the definations of like interface identifiers(IID) for Com.
There are two ways to write a windbg extension.First is using the windbg extension APIs.lets talk about it.
In using windbg extension you have to get the pointer to `` `WINDBG_EXTENSION_APIS` ``or ` ``WINDBG_EXTENSION_APIS64` ``for 64 bits windows using the `` `WindbgExtensionApisDllInit` `` function.

```
C
WINDBG_EXTENSION_APIS64 ExtensionApis;//the declaration of global variable where WINDBG_EXTENSION_APIS64 will be stored
USHORT MajorVersion;
USHORT MinorVersion;

//Thus function gets a pointer of WINDBG_EXTENSION_APIS64 and stores it in the global variable

void WindbgExtensionDllInit(PWINDBG_EXTENSION_APIS64 lpExtensionApis,USHORT major,USHORT minor){
    ExtensionApis=*lpExtensionApis;
    MajorVersion=major;
    MinorVersion=minor;
    }

```

This  function gets a pointer to this:
```
typedef struct _WINDBG_EXTENSION_APIS {

 ULONG nSize;

  PWINDBG_OUTPUT_ROUTINE lpOutputRoutine;

   PWINDBG_GET_EXPRESSION lpGetExpressionRoutine;

    PWINDBG_GET_SYMBOL lpGetSymbolRoutine;

     PWINDBG_DISASM lpDisasmRoutine;

      PWINDBG_CHECK_CONTROL_C lpCheckControlCRoutine;

       PWINDBG_READ_PROCESS_MEMORY_ROUTINE lpReadProcessMemoryRoutine;

        PWINDBG_WRITE_PROCESS_MEMORY_ROUTINE lpWriteProcessMemoryRoutine;

         PWINDBG_GET_THREAD_CONTEXT_ROUTINE lpGetThreadContextRoutine;

          PWINDBG_SET_THREAD_CONTEXT_ROUTINE lpSetThreadContextRoutine;

           PWINDBG_IOCTL_ROUTINE lpIoctlRoutine;

            PWINDBG_STACKTRACE_ROUTINE lpStackTraceRoutine;

            } WINDBG_EXTENSION_APIS, *PWINDBG_EXTENSION_APIS;

```
There is a reason why we name the global variable as ExtensionApis is because the header file ```wdbgexts.h``` defines some macros that refer to ExtensionApis to access the API pointers:

```
```
C

extern WINDBG_EXTENSION_APIS ExtensionApis;

#define dprintf (ExtensionApis.lpOutputRoutine)

#define GetExpression (ExtensionApis.lpGetExpressionRoutine)

#define CheckControlC (ExtensionApis.lpCheckControlCRoutine)

#define GetContext (ExtensionApis.lpGetThreadContextRoutine)

...

#define ReadMemory (ExtensionApis.lpReadProcessMemoryRoutine)

#define WriteMemory (ExtensionApis.lpWriteProcessMemoryRoutine)

#define StackTrace (ExtensionApis.lpStackTraceRoutine)

```
```
These macros enables enables windbg extension writters to eliminate using for example lpExtensionApis.WriteMemory and use WriteMemory instead.When using windbg extension apis it is mandatory to include WindbgExtensionDllInit function as your first Export,We will see that in a minute.
So lets about another function that is needed:
```ExtensionApiVersion```.This function returns a pointer of the ```EXT_API_VERSION ```structure and you can store it in a global variable also eg:
```
```
C
Ext_API_VERSION ApiVersion={6 //major,
                            1 //minor,
                            EXT_API_VERSION_NUMBER64 //revison,
                            0 //reserved
                            }

//now this is the function

PEXT_API_VERSION ExtensionApiVersion(void){
    return &ApiVersion;//stores it in the global variable ApiVersion
    }

```
```
So those two are the mandatory Functions to export so that windbg engine can be able to interpret your extension.

Obviously you can write your own functions that you can export also.There are some several ways of doing so.First you can do:
```
```
C
CPPMOD VOID myextension(

    HANDLE hCurrentProcess,

    HANDLE hCurrentThread,

    ULONG dwCurrentPc,

    ULONG dwProcessor,

    PCSTR args
    )
//where myextension is the name of your function

```
```
CPPMOD is a wrapper to __stdcall which is a calling convention which means the callee cleans the stack.HRESULT,WINAPI,STDMETHODCALLTYPE are also wrappers/are defined as __stdcall macros but (WINAPI is mostly used in defining Win32 Apis and STDMETHODCALLTYPE is used mostly in defination of COM interfaces such as QueryInterface,AddRef and so on.So you can use HRESULT in place of CPPMOD and it will work well also.
The second way of defining functions is using the DECLARE_API macro:
'''
'''
C
DECLARE_API(FunctionName){
    //do something
    }

```
```
DECLARE_API is a macro that is a wrapper to the first function.It is defines as

```
```
C
#define DECLARE_API(X) CPPMOD VOID x(HANDLE hCurrentProcess,HANDLE hCurrentThread,ULONG dwCurrentPc,ULONG dwProcessor,PCSTR args)
```
```

But since i am using gcc i used the HRESULT void Function to define my functions

using Windbg extension apis is easy since after you have defines the mandatory functions and exported them you can use raw functions such as WriteMemory,ReadMemory,Output and so on.


The second way of writting windbg extensiin is using COM interfaces and this is the one that i was interested in since if teaches you COM interface internals obviously i used gcc so i learned alot.
Using interfaces the you can still define the two mandatory functions(it worked for me) or you can use only one function,ie,```DebugExtensionInitialize``` which windows recommends to use now.It is defined as:

```
```
C
HRESULT CALLBACK DebugExtensionInitialize(
PULONG Version, PULONG Flags ) {
* Version = 1 ;
* Flags = 0 ;//always zero
return S_OK ;
}
```
```

When using interfaces you will not just call WriteMemory you have to use the interface provided by windows headers or define then yourself.I used gcc so i had rrrrrto define them myself.Lets walk throught it.
For COM IUNKNOWN interfaces(QueryInterface,AddRef,Release) to work they need interface identifiers(IID_##x) for the interface being manipulated.For Windbg,windows offers some interfaces(i am not going ti ytalk deeply about them here,maybe i will make a post one day about them):
1.```IDebugClient```-This interface offers function for starting and stopping debug sessions and others.eg AttachProcess

2.```IDebugControl```-As the name suggest offers function for controling the process.eg AddBreakPoint

3.```IDebugDataSpace```-it provides memory and data related functions eg ReadVirtual

4.```IDebugRegisters```-For registers eg GetDescription.

5.```IDebugSymbols```-Functions for debuggubing symbols eg GetImagePath

6.```IDebugSystemObjects```-queries the system and process being debuged eg GetCurrentProcessId

7.```IDebugAdvanced```-provides more functionality that is not defined by other interfaces eg GetThreadContext

So this are the interfaces that you work with when writting windbg extension.They haave so many functions offered by then.

Since i am using gcc i will  most definately define the vtables(virtual tables),COM uses Virtual tables internally(which are used in C++ using the virtual and can be defined in C also,story for another day).
I said COM IUNKNOWN interfaces uses IID to know which interface is being referenced.IID are globally unique GUID,meaning if you reference one IID COM cannot return another IID,one IID is unique no other GUID is like it.In gcc,the compiler doesn't have a knowledge about IIDs th at they are GUIDs so you have to make the compiler knowlegable of it and this is by using a macro named ```INUTGUID```:
```
```
C
#define INITGUID
```
```

This will make gcc to interpret IIDs as GUIDs,but still IIDs are not defined in gcc so we have to define them.
IID have a format of IID_##X X being the name of the inteface eg IID_IDebugClient
.__uuidof() function retrieves the GUID of the interface,so this makes it easy fir querying interfaces since you don't the whole 64 bit GUID you can get it using the name of it.So we define __uuidof as:

```
```
C
ifdef __GNUC__
#define __uuidof(x)&IID_##x
#else
#define __uuidof(x)IID_##x
#endif

```
```
i don't why gcc needs __uuidof to take &IID_##x i havent figured it out.

So after defining this macros the only thing you need is define the vtables.And before that in windbg extension  you have to create the IDebugClient interface first using DebugCreate so that you can use it to query other interfaces.
```
```
C
HRESULT DebugCreate(
  [in]  REFIID InterfaceId,
  [out] PVOID  *Interface
);
```
```
so the fisrt parameter of this function 
you can use __uuidof() to get the GUID eg

```
```
C
IDebugClient *client=0;
HRESULT hr=DebugCreate(__uuidof(IDebugClient),(void**)&client);
```
```
after getting thd IDebugClient Interface you can now get other interfaces using QueryInterface eg if we want to print something on windbg screen we have to use Output Function which you can get fron IDebugControl interface :
```
```
C
IDebugControl *Control=0;
HRESULT hr=client->vtbl->QueryInterface(client,__uuidof(IDebugControl),(void**)&control);
Control->vtbl->Output(control,DEBUG_CONTROL_NORMAL,"hello world\n");

```
```
so we use vtbl here to reference the interface vtables where it contains th addresses of the functions you are querying.


so this code what it does it just walks the heap from the address you gave it,it displays size,flags,state,if its a low fragmentation heap(a topic for another day) and so on.It is helpful for research vulnerability since you can see if the heap metadata such as size,flags,previous size and so on are corrupted by heap overflow and so on.

it is just a small program,i will be adding small functionality eventually.This code doesn't account of the SEGMENT_HEAP ince i wrote it on windows 7 so on modern windows it will not work because it will need some adjustment,since HEAP structure changed and there was an inclusion of SEGMENT_HEAP which is implemented entirely different from the Classic NT HEAP.Also the program uses offsets which are not the same accross builds and versions so it is good to look at the offset of your version and build.
https://www.vergiliusproject.com/ is a good resource for looking up offsets fro structures in windows also you can use windbg to look for the structure and calculate the offsets.

I would recomend if using it use it for learning and not for something soo serious.
For compiling it you need to include a .def file that has the exports that you will export.eg

```
```
def
LIBRARY Dumpheap.dll
EXPORTS 
    Function1
    Function2
    ....
    .Function n

```
```
so compiling you will do like so:

```gcc -g -shared pathoffile pathtodeffile -o pathtooutputdll```

using it in windbg,you will have to load it using the .load command eg
``` .load pathtodll```
and then since i queried the interfaces in one function in dumpheap.c ,ie,the init function i will have to run it furst before other functions:
```!init```
commands with "!" are imoorted from dlls,like if you have written a dll extension you must use "!" so that windbg engine can interpret it coming from outside source rather than its defined commands.
Then you can call other functions using "!" eg in my dumpheap.c file there is a function called "dumpheap" that implements the functionality of walking the heap and displaying relevant information.
```!dumpheap```

you can then unload the dll using .unload windbg command





