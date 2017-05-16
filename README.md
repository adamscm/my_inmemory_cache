# my_inmemory_cache
支持内存缓存操作,支持数据持久化,采用epoll实现异步处理,单进程多线程模型,独立线程处理数据持久化操作
make后生成服务端程序myInmemoryCache和客户端测试程序test_client
启动服务
./myInmemoryCache 端口号
客户端
./test_client 服务端部署IP 服务端部署端口号
客户端连接成功后即可进行set get 操作
比如 set a 1
     get a 服务端返回1
	 set a 23aa
	 get a 服务端返回23aa