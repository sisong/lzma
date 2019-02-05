// ThreadsP.c
//   for not _WIN32 ,used pthread
//   2019-02-05 : by housisong : for HDiffPatch
#include "ThreadsP.h"
#include "Threads.h"
#ifndef _WIN32

#include <pthread.h>
#include <assert.h>
#include <stdlib.h> // malloc free
#include <string.h> // memset

typedef struct CObject_t{
    WRes  (*wait)(HANDLE h);
    WRes (*close)(HANDLE* h);
} CObject_t;

WRes HandlePtr_Close(HANDLE* h){
    CObject_t* pobj=0;
    if ((0==h)||(0==*h)) return SZ_OK;
    pobj=(CObject_t*)(*h);
    return pobj->close(h);
}

WRes Handle_WaitObject(HANDLE h){
    CObject_t* pobj=0;
    assert(h!=0);
    pobj=(CObject_t*)h;
    return pobj->wait(h);
}


typedef struct CThread_t{
    CObject_t   base;
    pthread_t   tid;
} CThread_t;

static WRes _Thread_wait(HANDLE h){
    CThread_t*      self=(CThread_t*)h;
    void *          join_return=0;
    int             rt;
    assert(self!=0);
    assert(self->tid!=0);
    assert(self->base.wait==_Thread_wait);
    
    rt = pthread_join(self->tid,&join_return);
    if (rt==0)
        self->tid=0;
    return rt;
}

static WRes _Thread_close(HANDLE* h){
    CThread_t*      self=0;
    int             rt;
    assert(h!=0);
    self=(CThread_t*)(*h);
    assert(self!=0);
    assert(self->base.close==_Thread_close);
    
    rt=SZ_OK;
    if (self->tid!=0){
        rt=pthread_detach(self->tid);
        self->tid=0;
    }
    free(self);
    *h=0;
    return rt;
}

WRes Thread_Create(CThread* p,THREAD_FUNC_RET_TYPE (THREAD_FUNC_CALL_TYPE* func)(void *),LPVOID param){
    int             rt,rt2;
    pthread_t       tid=0;
    pthread_attr_t  attr;
    assert(p!=0);
    assert((*p)==0);
    {//set attr
        rt=pthread_attr_init(&attr);
        if (rt!=0) return rt;
        rt=pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);
        if (rt!=0) return rt;
    }
    rt=pthread_create(&tid,&attr,(void * (*)(void *))func,param);
    rt2=pthread_attr_destroy(&attr);
    if (rt!=0) return rt;
    if (rt2!=0){
        pthread_detach(tid);
        return rt2;
    }
    
    {
        CThread_t* self=(CThread_t*)malloc(sizeof(*self));
        if (self==0) return SZ_ERROR_MEM;
        self->base.wait =_Thread_wait;
        self->base.close=_Thread_close;
        self->tid = tid;
        *p=self;
        return SZ_OK;
    }
}


typedef struct CEvent_t{
    CObject_t       base;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    int             isManualReset;
    volatile int    isState;
} CEvent_t;

static WRes _Event_wait(HANDLE h) {
    CEvent_t*   self=(CEvent_t*)h;
    int         rt;
    int         rt2;
    assert(self!=0);
    assert(self->base.wait==_Event_wait);
    
    rt=pthread_mutex_lock(&self->mutex);
    if (rt!=0) return rt;
    rt2=SZ_OK;
    while (!self->isState){
        rt2=pthread_cond_wait(&self->cond,&self->mutex);
        if (rt2!=0) break;
    }
    if (!self->isManualReset)
        self->isState=0;
    rt=pthread_mutex_unlock(&self->mutex);
    if (rt2!=0) return rt2;
    return rt;
}

static WRes _Event_close(HANDLE* h) {
    CEvent_t*   self=0;
    int         rt,rt2;
    assert(h!=0);
    self=(CEvent_t*)(*h);
    assert(self!=0);
    assert(self->base.close==_Event_close);
    
    rt2=pthread_cond_destroy(&self->cond);
    rt=pthread_mutex_destroy(&self->mutex);
    free(self);
    *h=0;
    if (rt2!=0) return rt2;
    return rt;
}

static WRes _Event_Create(CEvent* p,int signaled,int isManualReset){
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    int             rt;
    assert(p!=0);
    assert((*p)==0);
    rt=pthread_mutex_init(&mutex,0);
    if (rt!=0) return rt;
    rt=pthread_cond_init(&cond,0);
    if (rt!=0){
        pthread_mutex_destroy(&mutex);
        return rt;
    }
    {
        CEvent_t* self=(CEvent_t*)malloc(sizeof(*self));
        if (self==0){
            pthread_cond_destroy(&cond);
            pthread_mutex_destroy(&mutex);
            return SZ_ERROR_MEM;
        }
        self->base.wait =_Event_wait;
        self->base.close=_Event_close;
        self->mutex =mutex;
        self->cond  =cond;
        self->isManualReset= isManualReset?1:0;
        self->isState      = signaled?1:0;
        *p=self;
        return SZ_OK;
    }
}

WRes Event_Set(CEvent* p) {
    CEvent_t*       self=0;
    int             rt,rt2;
    assert(p!=0);
    self=(CEvent_t*)(*p);
    assert(self!=0);
    
    rt=pthread_mutex_lock(&self->mutex);
    if (rt!=0) return rt;
    self->isState = 1;
    rt=pthread_cond_broadcast(&self->cond);
    rt2=pthread_mutex_unlock(&self->mutex);
    if (rt!=0) return rt;
    return rt2;
}

WRes Event_Reset(CEvent* p) {
    CEvent_t*       self=0;
    int             rt;
    assert(p!=0);
    self=(CEvent_t*)(*p);
    assert(self!=0);
    
    rt=pthread_mutex_lock(&self->mutex);
    if (rt!=0) return rt;
    self->isState = 0;
    return pthread_mutex_unlock(&self->mutex);
}

WRes ManualResetEvent_Create(CManualResetEvent* p, int signaled){
    return _Event_Create(p,signaled,1);
}

WRes ManualResetEvent_CreateNotSignaled(CManualResetEvent* p){
    return ManualResetEvent_Create(p,0);
}

WRes AutoResetEvent_Create(CAutoResetEvent* p, int signaled){
    return _Event_Create(p,signaled,0);
}

WRes AutoResetEvent_CreateNotSignaled(CAutoResetEvent* p){
    return AutoResetEvent_Create(p,0);
}


typedef struct CSemaphore_t{
    CObject_t       base;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    UInt32          maxCount;
    volatile UInt32 count;
} CSemaphore_t;

static WRes _Semaphore_wait(HANDLE h){
    CSemaphore_t*   self=(CSemaphore_t*)h;
    int             rt,rt2;
    assert(self!=0);
    assert(self->base.wait=_Semaphore_wait);
    
    rt=pthread_mutex_lock(&self->mutex);
    if (rt!=0) return rt;
    rt2=SZ_OK;
    while(self->count==0){
        rt2=pthread_cond_wait(&self->cond,&self->mutex);
        if (rt2!=0) break;
    }
    --self->count;
    rt=pthread_mutex_unlock(&self->mutex);
    if (rt2!=0) return rt2;
    return rt;
}

static WRes _Semaphore_close(HANDLE* h){
    CSemaphore_t*   self=0;
    int             rt,rt2;
    assert(h!=0);
    self=(CSemaphore_t*)(*h);
    assert(self!=0);
    assert(self->base.close=_Semaphore_close);
    
    rt2=pthread_cond_destroy(&self->cond);
    rt=pthread_mutex_destroy(&self->mutex);
    free(self);
    *h=0;
    return SZ_OK;
}

WRes Semaphore_Create(CSemaphore* p, UInt32 initCount, UInt32 maxCount){
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    int             rt;
    assert(p!=0);
    assert((*p)==0);
    rt=pthread_mutex_init(&mutex,0);
    if (rt!=0) return rt;
    rt=pthread_cond_init(&cond,0);
    if (rt!=0){
        pthread_mutex_destroy(&mutex);
        return rt;
    }
    
    {
        CSemaphore_t*   self=(CSemaphore_t*)malloc(sizeof(*self));
        if (self==0){
            pthread_cond_destroy(&cond);
            pthread_mutex_destroy(&mutex);
            return SZ_ERROR_MEM;
        }
        self->base.wait =_Semaphore_wait;
        self->base.close=_Semaphore_close;
        self->mutex   =mutex;
        self->cond    =cond;
        self->maxCount=maxCount;
        self->count   =initCount;
        *p=self;
        return SZ_OK;
    }
}

WRes Semaphore_ReleaseN(CSemaphore* p, UInt32 num){
    CSemaphore_t*   self=0;
    int             rt,rt2;
    UInt32          newCount;
    if (num==0) return SZ_ERROR_PARAM;
    assert(p!=0);
    self=(CSemaphore_t*)(*p);
    assert(self!=0);
    
    rt=pthread_mutex_lock(&self->mutex);
    if (rt!=0) return rt;
    newCount = self->count + num;
    if (newCount>self->maxCount){
        pthread_mutex_unlock(&self->mutex);
        return SZ_ERROR_PARAM;
    }
    self->count = newCount;
    rt2=pthread_cond_broadcast(&self->cond);
    rt=pthread_mutex_unlock(&self->mutex);
    if (rt2!=0) return rt2;
    return rt;
}

WRes Semaphore_Release1(CSemaphore* p){
    return Semaphore_ReleaseN(p,1);
}


typedef struct CCriticalSection_t{
    pthread_mutex_t     mutex;
} CCriticalSection_t;


WRes CriticalSection_Init(CCriticalSection* p){
    pthread_mutex_t mutex;
    int             rt;
    assert(p!=0);
    assert((*p)==0);
    rt=pthread_mutex_init(&mutex,0);
    if (rt!=0) return rt;
    
    {
        CCriticalSection_t* self=malloc(sizeof(*self));
        if (self==0){
            pthread_mutex_destroy(&mutex);
            return SZ_ERROR_MEM;
        }
        self->mutex=mutex;
        *p=self;
        return SZ_OK;
    }
}

void _def_DeleteCriticalSection(CCriticalSection* p){
    CCriticalSection_t* self=0;
    int                 rt;
    if ((p==0)||(*p)==0) return;
    self=(CCriticalSection_t*)(*p);
    rt=pthread_mutex_destroy(&self->mutex);
    *p=0;
    assert(rt==0);
}

void _def_EnterCriticalSection(CCriticalSection* p){
    CCriticalSection_t* self=0;
    int                 rt;
    assert(p!=0);
    self=(CCriticalSection_t*)(*p);
    assert(self!=0);
    rt=pthread_mutex_lock(&self->mutex);
    assert(rt==0);
}

void _def_LeaveCriticalSection(CCriticalSection* p){
    CCriticalSection_t* self=0;
    int                 rt;
    assert(p!=0);
    self=(CCriticalSection_t*)(*p);
    assert(self!=0);
    rt=pthread_mutex_unlock(&self->mutex);
    assert(rt==0);
}


//#ifdef MTCODER__USE_WRITE_THREAD
static pthread_mutex_t _g_inc_lock = PTHREAD_MUTEX_INITIALIZER;
LONG _def_InterlockedIncrement(LONG* lpAddend){
    //very slow , todo: changed to use __sync_fetch_and_add or atomic_inc_return ...
    LONG result;
    pthread_mutex_lock(&_g_inc_lock);
    result=*lpAddend;
    ++result;
    *lpAddend=result;
    pthread_mutex_unlock(&_g_inc_lock);
    return result;
}
//#endif

#endif
