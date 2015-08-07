## serverCron主要干了下面几件事情： ##
```
1）查看后台的snapshot或者AOF子进程是否完成，如果完成则由
   backgroundSaveDoneHandler或者backgroundRewriteDoneHandler
   来处理收尾工作。当然如果这个时候没有后台进程在干活并且满足
   一定条件的时候会调用rdbSaveBackground启动snapshot子进程。

2）查看是否有一些key已经过期，避免它们占用太多的内存空间   

3）如果VM打开并且已经使用的内存超过了阈值，则将一个部分key到
　　磁盘空间，当然这种事情发生时首先会从空闲队列中释放object

4）做一些replication相关的事情， 主要由syncWithMaster负责
```

相关函数：
```
closeTimedoutClients
htNeedsResize
tryResizeHashTables
incrementallyRehash
backgroundSaveDoneHandler
backgroundRewriteDoneHandler
updateDictResizePolicy
tryFreeOneObjectFromFreelist
syncWithMaster
rewriteAppendOnlyFileBackground
```

## Redis是一个内存存储系统，所以当内存紧张（比如当前使用的内存超过了配置文件中设置的最大值）的时候怎么办？ ##

这个时候主要做下面几件事情来应对这种情况：
```
1）释放空闲队列中预先分配的object，redis会预先分配1M个空闲object，具
   体可以参考tryFreeOneObjectFromFreelist

2）有一些key可能过期了，这个时候可以把它们占用的空间释放出来

3）如果1、2都搞不定那只能选择拒绝客户端的请求了
```

相关函数：
```
freeMemoryIfNeeded
tryFreeOneObjectFromFreelist
```

## Redis使用了AOF和snapshot机制来保证server挂了怎么办的问题，那它们是怎么实现的呢？ ##
```
正常情况下AOF很简单，就是针对写操作需要先在AOF把这个动作记录下来，但是这种简
单的append会导致AOF很大很大，可能只包含几个key的动作，所以这个时候就需要rewrite
AOF来帮忙, 通常rewrite AOF是由用户命令BGREWRITEAOF来驱动的，这时redis server会fork
一个子进程来rewrite AOF到一个临时文件中去，而父进程会将后继动作记录到内存中去，当
子进程完成rewrite AOF时，父进程负责merge内存中的记录到这个临时AOF中去，再重命名将
其扶正。
```
相关函数：
```
feedAppendOnlyFile
catAppendOnlyExpireAtCommand
catAppendOnlyGenericCommand

startAppendOnly
rewriteAppendOnlyFileBackground
rewriteAppendOnlyFile
```