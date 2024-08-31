# Http Message Queue
一个基于HTTP协议实现的内存消息队列示例项目，实现了创建topic、推送消息、按offset获取消息、以及删除topic操作。这个示例过于简单，只有获取消息这个接口使用了异步操作，因此仅适用于入门阶段的读者阅读。

若要实现一个更为复杂的示例，可以考虑将消息持久化到数据文件上，由于读写文件、创建删除topic都需要异步锁机制支持，上述四个操作便都需要真正的异步操作了。作者目前还不确定[coke](https://github.com/kedixa/coke)提供的机制是否可以方便地实现该复杂示例，有想法的读者可以在`issue`提出自己的思路。

## 使用示例
假设服务地址为`127.0.0.1`，端口为`8000`，则

### 创建topic
通过请求`http://127.0.0.1:8000/create?topic=discuss_coke&que_size=1234`创建一个名为`discuss_coke`的主题，最大消息保存个数为1234。

### 向topic推送数据
通过请求`http://127.0.0.1:8000/put?topic=discuss_coke`，并在HTTP消息体填充消息内容，来将消息推送至`discuss_coke`主题上。

### 从topic拉取数据
通过请求`http://127.0.0.1:8000/get?topic=discuss_coke&offset=10&timeout=2000&max=5`获取`discuss_coke`主题的数据，从偏移量`offset=10`开始至多读取`max=5`条，若该偏移量的数据已经过期则从最新的开始读取，若该偏移量还没有数据，则至多等待`timeout=2000`毫秒。

### 删除topic
通过请求`http://127.0.0.1:8000/delete?topic=discuss_coke`删除`discuss_coke`这个主题。

有了这些操作，便可以实现一个在线聊天室了，虽然服务端没有使用主动消息推送，但客户端可以通过指定偏移量来拉取新消息。

## 构建环境
GCC >= 13

## LICENSE
Apache 2.0
