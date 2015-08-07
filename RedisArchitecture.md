# 单线程异步处理事件，多线程处理IO

## Files Organize ##

  * anet.h/anet.c
```
网络通信
```

  * ae.h/ae.c,ae\_select.c/ae\_epoll.c/ae\_kqueue.c
```
事件处理
```

  * adlist.h/adlist.c,dict.h/dict.c, zipmap.h/zipmap.c
```
主数据结构
```

  * zmalloc.h/zmalloc.c
```
内存分配
```


  * sds.h/sds.c
```
动态字符串处理
```

  * redis.h/redis.c
```
主函数
```

## Code Skeleton ##

**main
  * initServerConfig
```
参考：redis.c 

初始化redisServer中的一部分fields的value，下面仅列出对理解影响比较大的field.
static struct redisServer server;

dbnum          => REDIS_DEFAULT_DBNUM;
port           => REDIS_SERVERPORT
dbfilename     => dump.rdb
appendfilename => appendonly.aof
```
  * initServer
    * listCreate
```
参考：adlist.c
```
    * aeCreateEventLoop
```
参考：ae.c

为aeEventLoop结构体分配空间，对它的一些fields做一个简单的初始化
```
    * anetTcpServer
```
参考：anet.c

调用socket, bind, listen创建一个负责监听request的套接字
```
    * dictCreate
```
参考：dict.c

为dict结构体分配空间，对它的一些fields做一个简单的初始化
```
    * aeCreateTimeEvent
```
参考：ae.c

为aeTimeEvent结构体分配空间，初始化，然后将它放到eventLoop的timeEventHead链表的头，完成对timer event的注册
```
      * serverCron
```
参考：dict.c
```
        * closeTimedoutClients
```
参考：dict.c

Close connections of timedout clients
```
        * if
```
server.bgsavechildpid != -1 || server.bgrewritechildpid != -1

Check if a background saving or AOF rewrite in progress terminated

```
          * backgroundSaveDoneHandler
```

```
          * backgroundRewriteDoneHandler
```
```
          * updateDictResizePolicy
```
```
        * else
```
If there is not a background saving in progress check if
we have to save now
```
          * rdbSaveBackground
```
参考：redis.c
```
            * rdbSave
```
参考：redis.c
Save the DB on disk
```
    * aeCreateFileEvent
```
参考：ae.c

初始化fd对应的aeFileEvent，完成对fd event的注册
```
      * acceptHandler
```
参考：redis.c

负责处理客户端的request
```
        * anetAccept
```
参考：anet.c

调用accept建立和客户端的连接
```
          * createClient
```
参考：redis.c

为redisClient结构体分配空间，初始化，以后所以和客户端打交道用到的数据都可以在这里找到。
```
            * anetNonBlock
```
参考：anet.c
```
            * anetTcpNoDelay
```
参考：anet.c
```
            * aeCreateFileEvent
```
参考：ae.c
```
              * readQueryFromClient
```
参考：redis.c

作为fd event的回调函数，当一个描述符可读的时候被调用，读取客户端发送来的数据，然后调用下面的函数处理，参数当然就是redisClient了。
```
                * processInputBuffer
```
参考：redis.c
```
                  * processCommand
```
参考：redis.c
```
                    * lookupCommand
```
参考：redis.c

Search the redisCommand from cmdTable according to the command name.
```
                      * queueMultiCommand
```
参考：redis.c
```
                      * addReply
```
参考：redis.c

Send apply 'obj' to client
Register the reply to client event if not exist
Add the obj to the tail of reply data list
The callback of this event is sendReplyToClient
```
                        * sendReplyToClient
```
参考：redis.c

Send the data using sendReplyToClient or write directly.
```
                          * sendReplyToClientWritev
```
参考：redis.c

Call writev to send multi-buf data
```
                          * write
                        * listAddNodeTail
```
参考：adlist.c
```
                    * call
            * selectDb
```
参考：redis.c
```
            * listAddNodeTail
```
参考：adlist.c
```
    * vmInit
```
参考：adlist.c
Initial the following fields in redisServer

    /* Virtual memory state */
    FILE *vm_fp;
    int vm_fd;
    off_t vm_next_page; /* Next probably empty page */
    off_t vm_near_pages; /* Number of pages allocated sequentially */
    unsigned char *vm_bitmap; /* Bitmap of free/used pages */
    time_t unixtime;    /* Unix time sampled every second. */
    /* Virtual memory I/O threads stuff */
    /* An I/O thread process an element taken from the io_newjobs queue and
     * put the result of the operation in the io_processed list. While the
     * job is being processed, it's put on io_processing queue. */
    list *io_newjobs; /* List of VM I/O jobs yet to be processed */
    list *io_processing; /* List of VM I/O jobs being processed */
    list *io_processed; /* List of VM I/O jobs already processed */
    list *io_ready_clients; /* Clients ready to be unblocked. All keys loaded */
    pthread_mutex_t io_mutex; /* lock to access io_jobs/io_done/io_thread_job */
    pthread_mutex_t obj_freelist_mutex; /* safe redis objects creation/free */
    pthread_mutex_t io_swapfile_mutex; /* So we can lseek + write */
    pthread_attr_t io_threads_attr; /* attributes for threads creation */
    int io_active_threads; /* Number of running I/O threads */
    int vm_max_threads; /* Max number of I/O threads running at the same time */
    /* Our main thread is blocked on the event loop, locking for sockets ready
     * to be read or written, so when a threaded I/O operation is ready to be
     * processed by the main thread, the I/O thread will use a unix pipe to
     * awake the main thread. The followings are the two pipe FDs. */
    int io_ready_pipe_read;
    int io_ready_pipe_write;
    /* Virtual memory stats */
    unsigned long long vm_stats_used_pages;
    unsigned long long vm_stats_swapped_objects;
    unsigned long long vm_stats_swapouts;
    unsigned long long vm_stats_swapins;
```
      * aeCreateFileEvent
```
Create the file event for io_ready_pipe_read, the callback 
is vmThreadIOCompletedJob, so when the io_read_pipe_read
readable, the callback will be called. 
```
        * vmThreadedIOCompletedJob
```
/*
 * Every time a thread finished a Job, it writes a byte into the write side
 * of an unix pipe in order to "awake" the main thread, and this function
 * is called.
 *
 * Note that this is called both by the event loop, when a I/O thread
 * sends a byte in the notification pipe, and is also directly called from
 * waitEmptyIOJobsQueue().
 *
 * In the latter case we don't want to swap more, so we use the
 * "privdata" argument setting it to a not NULL value to signal this
 * condition.
 */
```
  * loadAppendOnlyFile
```
参考:redis.c

Only when the value of "server.appendonly" is true to execute this method

Replay the append log file. On error REDIS_ERR is returned. On non fatal
error (the append only file is zero-length) REDIS_OK is returned. On
fatal error an error message is logged and the program exists.
```
    * redis\_fstat
```
#defined redis_fstat fstat
```
    * createFakeClient
```
The Redis commands are always executed in the context of a client, so in
order to load the append only file we need to create a fake client.
```
    * while statement
```
Read a line from append only file
Command lookup using "lookupCommand"
Call redisCommand's proc method to execute the command
Discard the reply objects list from the fake client
Clean up, ready for the next command
Handle swapping while loading big datasets when VM is on
```
    * freeFakeClient
```
Free the memory of redisClient
```**

  * rdbLoad
```
参考:redis.c
```
  * aeMain
    * aeProcessEvents
```
参考：ae.c
Process every pending time event, then every pending file event
(that may be registered by time event callbacks just processed).
Without special flags the function sleeps until some file event
fires, or when the next time event occurs (if any).

If flags is 0, the function does nothing and returns.
if flags has AE_ALL_EVENTS set, all the kind of events are processed.
if flags has AE_FILE_EVENTS set, file events are processed.
if flags has AE_TIME_EVENTS set, time events are processed.
if flags has AE_DONT_WAIT set the function returns ASAP until all
the events that's possible to process without to wait are processed.
```
  * aeDeleteEventLoop
```
参考：ae.c
```