#Taste the redis first.

## Summary ##

Five minutes howto on how to get started with Redis

## Steps ##

**Step#1: Obtain the latest version of redis 2.0.4**
```
shawny@ubuntu:~/software$ wget http://redis.googlecode.com/files/redis-2.0.4.tar.gz
```

**Step#2: Untar the tar.gz and compile it using make**
```
shawny@ubuntu:~/software$ tar xvzf redis-2.0.4.tar.gz
shawny@ubuntu:~/software$ cd redis-2.0.4
shawny@ubuntu:~/software/redis-2.0.4$ make
```

**Step#3: Run the redis server**
```
shawny@ubuntu:~/software/redis-2.0.4$ ./redis-server 
[3758] 10 Dec 19:07:49 # Warning: no config file specified, using the default config. In order to specify a config file use 'redis-server /path/to/redis.conf'
[3758] 10 Dec 19:07:49 * Server started, Redis version 2.0.4
[3758] 10 Dec 19:07:49 # WARNING overcommit_memory is set to 0! Background save may fail under low memory condition. To fix this issue add 'vm.overcommit_memory = 1' to /etc/sysctl.conf and then reboot or run the command 'sysctl vm.overcommit_memory=1' for this to take effect.
[3758] 10 Dec 19:07:49 * The server is now ready to accept connections on port 6379
[3758] 10 Dec 19:07:49 - 0 clients connected (0 slaves), 533436 bytes in use
[3758] 10 Dec 19:07:54 - 0 clients connected (0 slaves), 533436 bytes in use
```

**Step#4: Play with the built in client "redis-cli"**
```
shawny@ubuntu:~/software/redis-2.0.4$ ./redis-cli set user shawny
OK
shawny@ubuntu:~/software/redis-2.0.4$ ./redis-cli get user
"shawny"
```