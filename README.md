# 本项目用作个人常用工具(能做出来的话)
---
## 1. 文字编码转换

### 所需功能：
- 提供GBK2312、UTF-8、ShiftJIS之间互相转换的功能
- 可对多个文件进行统一处理
- 生成的文件自动存放到“日期-时间-原编码-目标编码”的文件夹中
- 显示日志输出

## 2. 模型转换

### 功能
- 使用assimp将模型转换为json或二进制meshdata文件
- 可以传递特殊的处理flag

### 命令
      -type : file style to save info 
            json-pretty :   convert into formated json 
            json-normal :   convert into unformated json 
            binary :        convert into binary file  

      -flag : the flag pass to assimp to process mesh 
            left-hand :     process mesh to LH coord 
            right-hand :    process mesh to RH coord 
            optimize :      process with optimzie flag 

      -input : the model file you want to convert 
            [file name] :   the file's name without path 

## 3. 测试用的基于TCP聊天小工具

### 声明

- 安全性为0，不作任何保证，谨慎输入任何敏感数据
- 开发者不对任何内容负责
- 服务器无备份
- 服务器弱鸡，随时可能炸
- 仅支持x64的windows
- 非简体中文windows系统下，需要代码页936的支持

### 用户手册

- 房间号随意，但要在4字节整数的范围内
- 相同房间号的用户可以共同聊天
- 用户名16字节以内，重名需要重启client程序，似乎不会引起大问题懒得改了
- 发送消息控制在512字节以内，可以更高但忘记算极限值了，不会引起大问题但会丢失消息的数据
- 新消息不会自动刷新在屏幕上
- 敲击回车发送消息后会刷新，消息发送栏为空并敲击回车也会刷新
- 总之有事没事多敲回车
- 可用命令
  - `/MEMBER`：查看当前房间有哪些在线用户
  - `/QUIT`：退出当前房间