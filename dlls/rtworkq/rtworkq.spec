@ stdcall RtwqAddPeriodicCallback(ptr ptr ptr)
@ stdcall RtwqAllocateSerialWorkQueue(long ptr)
@ stdcall RtwqAllocateWorkQueue(long ptr)
@ stdcall RtwqBeginRegisterWorkQueueWithMMCSS(long wstr long long ptr ptr)
@ stub RtwqBeginUnregisterWorkQueueWithMMCSS
@ stdcall RtwqCancelDeadline(long)
@ stub RtwqCancelMultipleWaitingWorkItem
@ stdcall RtwqCancelWorkItem(int64)
@ stdcall RtwqCreateAsyncResult(ptr ptr ptr ptr)
@ stub RtwqEndRegisterWorkQueueWithMMCSS
@ stub RtwqEndUnregisterWorkQueueWithMMCSS
@ stub RtwqGetPlatform
@ stdcall RtwqGetWorkQueueMMCSSClass(long ptr ptr)
@ stdcall RtwqGetWorkQueueMMCSSPriority(long ptr)
@ stdcall RtwqGetWorkQueueMMCSSTaskId(long ptr)
@ stdcall RtwqInvokeCallback(ptr)
@ stdcall RtwqJoinWorkQueue(long long ptr)
@ stdcall RtwqLockPlatform()
@ stdcall RtwqLockSharedWorkQueue(wstr long ptr ptr)
@ stdcall RtwqLockWorkQueue(long)
@ stub RtwqPutMultipleWaitingWorkItem
@ stdcall RtwqPutWaitingWorkItem(long long ptr ptr)
@ stdcall RtwqPutWorkItem(long long ptr)
@ stdcall RtwqRegisterPlatformEvents(ptr)
@ stdcall RtwqRegisterPlatformWithMMCSS(wstr ptr long)
@ stdcall RtwqRemovePeriodicCallback(long)
@ stdcall RtwqScheduleWorkItem(ptr int64 ptr)
@ stdcall RtwqSetDeadline(long int64 ptr)
@ stdcall RtwqSetDeadline2(long int64 int64 ptr)
@ stdcall RtwqSetLongRunning(long long)
@ stdcall RtwqShutdown()
@ stdcall RtwqStartup()
@ stdcall RtwqUnjoinWorkQueue(long long)
@ stdcall RtwqUnlockPlatform()
@ stdcall RtwqUnlockWorkQueue(long)
@ stdcall RtwqUnregisterPlatformEvents(ptr)
@ stdcall RtwqUnregisterPlatformFromMMCSS()
