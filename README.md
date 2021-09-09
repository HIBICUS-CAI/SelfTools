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
