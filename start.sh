#!/usr/bin/env sh

# 下载编译
git clone https://github.com/hewei-nju/MyTinyWebServer.git
cd MyTinyWebServer
make all

# 修改cgi目录下文件权限，允许可执行 
sudo chmod 755 cgi/post.cgi

# 展示可访问的链接
echo "你可能访问需要使用的链接："
echo "http://localhost:8080"
echo "http://localhost:8080/post.html"

# 默认在8080端口运行httpd服务
echo "Usage: "
echo "1. ./webserver <IP> <Port>"
echo "2. ./webserver <Port>"
./webserver 8080