来源于：http://antirez.com/post/redis-virtual-memory-story.html

If you are reading this article probably you already know it: Redis is an in-memory DB. It's persistent, as it's disk backed, but disk is only used to persist, all the data is taken in the computer RAM.

I think the last few months showed that this was not a bad design decision. Redis proved to be very fast in real-world scenarios where there is to scale an unhealthy amount of writes, and it is supporting advanced features like Sorted Sets, and many other complex atomic operations, just because it is in memory, and single threaded. In other words, some of the features supported by Redis tend to be very complex to implement if there is to organize data on disk for fast access, and there are many concurrent threads accessing this data. The Redis design made this two problems a non issue, with the drawback of holding data in memory.

I really think to take data in memory is the way to go in many real world scenarios, as eventually your most accessed data must be in memory anyway to scale (think at the memcached farms many companies are running in this moment). But warning. I said most accessed. Too many datasets have something in common, they are accessed in a long tail fashion, that is, a little percentage of the dataset will get the majority of the queries (let's call it the hot spot). Still from time to time even data outside the hot spot is requested. With Redis we are forced (well, actually were forced) to take all the data in memory, and it's a huge waste as actually most of the times only our hot spot is stressed. So the logical question started to be more and more this: is there a way to free the memory used by rarely accessed data?
## Virtual Memory ##
Virtual Memory is an idea originated in the operating systems world, 50 years ago. It is probably one of the few non trivial OS ideas that many non tech people are aware of, in some way: the swap file is a famous object, and most Windows power users more or less understand how it works.

Basically the memory is organized in pages, that are usually 4096 bytes in size. The OS is able to transfer this pages from memory to disk to free memory. When an application will try to access an address that maps to the physical memory page that was transfered on disk, the processor will call a special function that is in charge of loading such a page in memory, so that the accessing program can continue the execution.

OSes will not swap memory only when they are out of memory, but even when there is still some free memory, as more free memory can always be used for a very precious thing: disk cache, and this is a win if the pages we transfer on the swap file were rarely accessed.

So the question is: Why Redis can't just use the OS Virtual Memory? (instead to invent its own VM at application level?). There are two main reasons:

  * The OS will only swap pages rarely used. A page is 4096 bytes. Redis uses hash tables, object sharing and caching, so a single Redis "value" (like a Redis List or Set) can be physically allocated across many different pages. The reverse is also true: a physical page will likely contain objects about many Redis keys. Basically even if just 10% of the dataset is actively used, probably all the memory pages are accessed. Maybe most memory pages will contain only a few bytes of our hot, frequently used data, but even a byte for page is enough for preventing swapping, or to force the OS to transfer back and forth memory pages from disk to memory if it's out of memory.
  * Redis objects, both simple and complex, take a lot more space when they are stored in RAM, compared to the space they take serialized on disk. On disk there are no pointers, nor meta data. An object is usually even 10 times smaller serialized on disk, as Redis is able to encode the objects stored on disk pretty well. This means that the Redis application-level VM needs to perform ten times less disk I/O compared to the OS VM, for the same amount of data.

While the OS cache can't help a lot, the idea behind Virtual Memory is very helpful. All I needed to do was to move the concept of Virtual Memory from kernel space to use space.
## Virtual Memory: the Redis way ##
There are many design details about implementing Virtual Memory in a key-value store, but well, the basic concept is pretty straightforward: when we are out of memory, let's transfer values belonging to keys not recently used from memory to disk. When a Redis command will try to access a key that is swapped out, it is loaded back in memory.

It's as simple as that, but in the above description there is the first of many design decisions: only values are swapped, not keys. This is actually the direct result of another much more important design principia I made at the start: dealing with in memory keys should be more or less as fast as when VM is disabled.

What this means is that you need to have enough RAM at least to hold all the keys objects, and this is the bad news, the good one is: Redis will be mostly as fast as you know it is when accessing in-memory keys. So if your dataset will have the famous "long tail" alike access pattern and your hot spot fits the available RAM, Redis will be as fast as it is with VM disabled.

Ok, it's time to show some number I guess, so you can start to make your math about the real world impact of Redis VM and when it is practical and when still too much memory is needed.
```
VM off: 300k keys, 4096 bytes values: 1.3G used
VM on:  300k keys, 4096 bytes values: 73M used
VM off: 1 million keys, 256 bytes values: 430.12M used
VM on:  1 million keys, 256 bytes values: 160.09M used
VM on:  1 million keys, values as large as you want, still: 160.09M used
```
Guess what? With VM on (and configuring Redis VM in order to use as little memory as possible), it does not matter how big the value is. 1 million keys will always use 160 MB. You can store huge lists or sets inside, or tiny string values. Every value will be swapped out, but the keys and the top level hash table, will still use RAM, as well as the "page table bitmap", that is a bit array of bits in the Redis memory containing information about used / free pages in the swap file.

So a very important question is, when VM is enabled, how much memory we'll use for every additional million of keys? More or less 160 MB for million of keys, so at minimum you need:
```
1M keys: 160 MB
10M keys: 1.6 GB
100M keys: 16 GB
```
If you have 16 GB of RAM you can store 100M of keys, and every key can contain values as large and complex as you want (Lists, Sets, JSON encoded objects, and so forth) and the memory requirements will not change.

Think at this: even with MySQL it is not trivial to have a database with 100 million rows with less than 16 GB of RAM, but with the top-level keys in memory the speed gain is big.
## A reversed memcached ##
When I started to work at Redis one year ago I often compared it to memcached, saying "it's like memcached, but persistent and with more ops" in order to tell people what Redis was about.

Not that this description was wrong from a pragmatic point of view, but in some philosophical sense Redis with VM is the exact contrary of MySQL + memcached.

Using memcached in order to cache SQL queries is a well established pattern. My SQL DB is slow, so I write an application layer to take the frequently accessed data in memcached (handling invalidation by hand), so I can query this faster cache instead of the DB. The idea is to take data on disk, but to cache the hotspot in memory for fast access.

Redis + VM is exactly the reverse. You take your data in memory, but what is not the hot spot is disk-backed in order to free mem for more interesting data. In both models the frequently accessed data will stay in memory, but the process is reversed, with the following benefits:

  * There is no invalidation to do. There is only one object we need to interact with, Redis. Data is not duplicated in two places, like in MySQL + memcached.
  * This model can scale writes as well as it can scale reads. MySQL + memcached can mainly scale read queries.
  * Once you write the memcached layer in your application, what you discover is that after all you are trying to access more and more data by unique key, or sort off: parametrized data is not handy to cache if the space of the parameters is large enough, and invalidation is crazy. Even to cache a simple pagination query can be hard, go figure with more complex stuff. So most benefits about SQL are lost in some way, you are silently turning your application into a key-value business! But memcached can't offer the higher level operations Redis is able to offer. To return to the pagination example, LRANGE and ZRANGE are your friends.

## The code ##
I implemented VM in two stages. The first logical step was to start with a blocking implementation, given that Redis is single threaded, that is, an implementation where keys are swapped out blocking all the other clients when we are out of memory (but swapping just as many objects as needed to return to the memory limits, so it actually does not appear to block the server). The blocking implementation also loads keys synchronously when a client is accessing a swapped out key (or better, a key associated to a swapped out value).

This implementation took very little time, as I used the same functions to serialize and unserialized Redis objects in Redis .rdb files (used in order to persist on disk). A few more details:

  * The swap file is divided in pages, the page size can be configured.
  * The page allocation table is taken in memory. It's a bitmap so every page takes 1 bit of actual RAM.
  * When VM is enabled, Redis objects are allocated with a few more fields, one of this is about the last time the object was accessed. So when Redis is out of memory and there is something to swap, we sample a few random objects from the dataset, and the one with the higher swappability is the one that will be transfered on disk. The swappability is currently computed using the formula Object.age\*Logarithm(Object.used\_memory).
  * The page allocation algorithm uses an algorithm I found reading the source code of the Linux VM system. Basically we try to allocate pages sequentially up to a given limit. When this limit is reached we start from page 0. This tries to improve locality. I added another trick: if I can't find free pages for a while, I start to fast forward with random jumps.
  * When Redis fork()s in order to save the dataset on disk (Redis uses copy-on-write semantic in order to take the snapshot of the DB) VM is suspended: only loads are allowed, writes are blocked. So the child can access the VM file without troubles. The same happens when the Append Only File is enabled and you issue a BGREWRITEAOF command.

## I/O threads ##
The blocking implementation worked very well, but in the real world there are applications where it is not good at all. It's perfect if you are using Redis with few clients to perform batch computations, but what about web applications with N clients? To wait for blocked clients to load stuff from disk before to continue is hardly an acceptable scenario.

Redis is a single-threaded multiplexing server, so a possible solution was to use non blocking disk I/O. I didn't liked enough this solution for a reason: it's not just a matter of I/O, also to serialize / unserialize the Redis objects to/from the disk representation is a slow CPU intensive operation with lists or sets composed of many elements. The last resort was what everybody tries to avoid (and for good reasons!): multi-threading programming.

There are two obvious ways to do this: serve every client with a different thread, or just make the VM I/O stuff threaded. I picked the second for two reasons: to make the implementation simpler and self contained (that is, outside the VM subsystem, no synchronization problems at all), and to retain the raw speed of the single threaded implementation when there was to access non swapped values.

So the final design is that the main thread communicate with a configurable number of I/O threads with a queue of I/O jobs. When there is a value to swap, an I/O job to swap the key is put in the queue. When there is a value to load because a client is requesting it, the client is suspended, an I/O job to load the key back in memory is added to the queue, and when all the keys needed for a given client are loaded the client is "resumed".

Basically the main thread puts I/O jobs in the io\_newjobs queue. After this jobs are processed, the I/O threads put the I/O jobs (filled with additional data) in the io\_processed queue. This processed jobs are post-processed by the main thread in order to change the status of the keys from swapped to in-memory or vice versa and so forth.
Our main trick
To resume a client that is in the middle of a command exectuion is hard, but there was a simple solution, a probabilistic one.

When a client issues a command, like: GET mykey, we scan the arguments looking for swapped keys. If there is at least one swapped key, the client is suspended before the command is executed at all. Once the keys are back in memory the client is resumed.

This trick allows to reduce the complexity a lot, but it is just probabilistic. What if once we resume a client a key is swapped again as we are in hard out of memory conditions? What about the "SORT BY" command that will access keys we can't guess beforehand? Well, that's simple: if a given key is swapped for some reason, Redis reverts to the blocking implementation.

The unblocking VM is a blocking VM with the trick of loading the keys in I/O threads thanks to static command analysis. As simple as that, and works very well for all the commands but SORT BY that is a slow operation anyway.
## Still too complex ##
The actual implementation is much more complex than that as you can guess. What happens if a value is being swapped off by an I/O thread while a client is accessing it? And so forth. There was to design the system so that I/O operations can be invalidated at any time, and this was tricky.

After the VM, I lost my feeling that Redis was trivial to gasp by the casual coder just reading the source code. Now it's 13k lines of code and there are many things to understand. Some functions are a few lines, but there are a lot of comments just to explain what's going on. Just an example, from the function in charge of jobs invalidation:
```
                switch(i) {
                case 0: /* io_newjobs */
                    /* If the job was yet not processed the best thing to do
                     * is to remove it from the queue at all */
                    freeIOJob(job);
                    listDelNode(lists[i],ln);
                    break;
                case 1: /* io_processing */
                    /* Oh Shi- the thread is messing with the Job:
                     *
                     * Probably it's accessing the object if this is a
                     * PREPARE_SWAP or DO_SWAP job.
                     * If it's a LOAD job it may be reading from disk and
                     * if we don't wait for the job to terminate before to
                     * cancel it, maybe in a few microseconds data can be
                     * corrupted in this pages. So the short story is:
                     *
                     * Better to wait for the job to move into the
                     * next queue (processed)... */

                    /* We try again and again until the job is completed. */
                    unlockThreadedIO();
                    /* But let's wait some time for the I/O thread
                     * to finish with this job. After all this condition
                     * should be very rare. */
                    usleep(1);
                    goto again;
                case 2: /* io_processed */
                    /* The job was already processed, that's easy...
                     * just mark it as canceled so that we'll ignore it
                     * when processing completed jobs. */
                    job->canceled = 1;
                    break;
                }

```

(Nazi Grammar Is Not Happy, I know). The complexity is self contained, but still there are a number of non trivial issues to understand for an external programmer in order to hack with the VM.

Fortunately the VM needs very little maintenance work, as the trick of using the same serialization format used to persiste on disk completely decoupled it from the other Redis subsystems. Want to implement a new type for Redis? Just write the commands to work with this new type and the functions to load/save it in the .rdb file and you are done. The VM will do the rest without your help.

Ok this article is already too long. I hope that Redis 2.0.0 will be released as stable code in two or three months at max. The VM needs a few more weeks of work and testing, but now it is working well and I encourage you to give it a try in development environment if you think you'll run out of memory in short time without it ;)

-------------代码分析分割线--------------

每一个IO thread都是通过如下流程创建的，线程入口是IOThreadEntryPoint:

queueIOJob (This function must be called while with threaded IO locked) -> spawnIOThread -> IOThreadEntryPoint

IO thread entry, it is a worker, so it will get the iojob from queue - io\_newjobs to process it.

  1. Move the iojob from io\_newjobs to io\_processing
  1. According the iojob type decide the things to do, if the type is "REDIS\_IOJOB\_LOAD", then it will call "vmReadObjectFromSwap -> rdbLoadObject" to read the object from swap file; if the type is "REDIS\_IOJOB\_PREPARE\_SWAP", then it will call "rdbSavedObjectPages" to calculate the number of pages which used to store the object; if the type is "REDIS\_IOJOB\_DO\_SWAP", then it will call "vmWriteObjectOnSwap" to write the object to swap file.
  1. Move the iojob from io\_processing to io\_processed
  1. Signal the main thread through write a character X to pipe "io\_ready\_pipe\_write"

vmThreadedIOCompletedJob

What's the functional of it?

Every time a thread finished a job, it writes a byte 'x' from the write side of an unix pipe in order to "awake" the main thread, then this function is called.

Who will call it?

1) vmInit -> aeCreateFileEvent -> vmThreadedIOCompletedJob
2) waitEmptyIOJobsQueue -> vmThreadedIOCompletedJob

How it was implement?

For every byte we read from the read side of the pipe, there is one I/O job completed to process, so it will loop read the read side of the pipe, get the first job from queue - io\_processed, if the job is marked as canceled, just ignore it, then if the type of job is "REDIS\_IOJOB\_LOAD", changed the status of the key, e.g.: set key->storage = REDIS\_VM\_MEMORY, it says that that key has been loaded from disk to memory, so we can call handleClientsBlockedOnSwappedKey to handle the clients which waiting for this key to be loaded; if the type of job is "REDIS\_IOJOB\_PREPARE\_SWAP", we call vmFindContiguousPages to get the amount of pages required to swap this object, only when there isn't background child process (BGSAVE, BGAEOREWRITE) and get the swap pages success, we change the type of job to REDIS\_IOJOB\_DO\_SWAP and call queueIOjob to insert it into io\_newjobs; if the type of job is "REDIS\_IOJOB\_DO\_SWAP", change the status of the key, e.g.: key->storage = REDIS\_VM\_SWAPPED, put a few more swap requests in queue if we are still out of memory through calling vmSwapOneObjectThreaded.

vmCancelThreadedIOJob

What's the functional of it?

Remove the specified object from the threaded I/O queue if still not
processed, otherwise make sure to flag it as canceled.

Who will call it?

vmLoadObject -> vmCancelThreadedIOJob (If we are loading the object in background, stop it, we need to load this object synchronously ASAP.)

waitForSwappedKey -> vmCancelThreadedIOJob (The client is just request the swapping key, so we need cancel the swapping)

How it was implement?

First the type of cancel iojob should be REDIS\_VM\_LOADING or REDIS\_VM\_SWAPPING, because the iojob maybe exist in three queue - io\_newjobs, io\_processing or io\_processed, so we need search for a matching key in each of the queue, if the iojob was cancel, we continue to check the next one; if we get the job which key is equal to the search key, then mark the pages as free since the swap didn't happened or happened but is noew discarded, if the iojob was in io\_newjobs, we only need free the job, remove it from list; if the iojob was in io\_processing, we need sleep and try again; if the iojob was in io\_processed, the job was alread processed, just mark it as canceld so that we will ignore it when processing completed jobs. Finally we have to adjust the storage type of the object in order to "UNDO" the operation, e.g.: REDIS\_VM\_LOADING -> REDIS\_VM\_SWAPPED, REDIS\_VM\_SWAPPING -> REDIS\_VM\_MEMORY