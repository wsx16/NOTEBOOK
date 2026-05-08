autosar架构：

应用层（Appl）

（SWC）（SWC）（SWC）

|

运行时环境（RTE）

|

基础软件层（BSW）

（服务层）（ECU抽象层）（微控制器抽象层）（复杂驱动）

|

硬件（hardware）

can通信

特点：

1.差分信号传输

2.通过消息优先级显隐性仲裁

3.两端有120欧姆的终端电阻，防止信号反射

4.通过不同的帧种类（数据帧，遥控帧，错误帧，过载帧，帧间隔）传递不同信息

5.多主控制，所有单元都可以发送消息

6.系统柔软性，易于扩展，总线可连接单元受到时间延迟和电器负载限制

![](https://pcn9vhf9gp4v.feishu.cn/space/api/box/stream/download/asynccode/?code=MjhhN2FjNWM3MWJmMmJmMmYxZTRjMjU3ZWNmNTJiYmFfeXowdm94WEoyZW1uSGc3alU4TlF1Znl0M0lMRE9vQzJfVG9rZW46QmJBdGIwb2Y1b1JMeUJ4MnRxZGNhWTVBbmpoXzE3NzgyMjc3MTQ6MTc3ODIzMTMxNF9WNA)

![](https://pcn9vhf9gp4v.feishu.cn/space/api/box/stream/download/asynccode/?code=ZWI2MDY2ZmJlNjBlZGZjZTMxNzJjZWFkNzk5MzkzY2ZfQ1FKc01DYXQ0cWJId2ZEbTk3bWI3WjB5Z3JNNmJBeHdfVG9rZW46WFl4dWJrMmRHb24zNVh4c1JIbGN5cnBNbk1nXzE3NzgyMjc3MTQ6MTc3ODIzMTMxNF9WNA)

通信协议架构

PDU：协议数据单元

COM模块：将信号装到i-pdu发送与接受，控制信号发送，为信号提供路由功能，将i-pdu信号打包发送给i-pdu，发送请求应答。

PDUR模块：作为PDU的网关功能，为通信接口模块，传输协议模块，诊断通信模块，通信模块提供PDU的路由服务。上层基础软件模块与应用不关心网络细节。

  

EcuC

CAN通信协议栈各层间以PDU传输，通过Global PDU将不同层的PDU关联起来，且其不属于任何BSW模块，通过EcuC来配置信息