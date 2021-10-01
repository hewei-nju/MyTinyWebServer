#!/usr/bin/env sh

# 下载编译
git clone https://github.com/hewei-nju/MyTinyWebServer.git
cd MyTinyWebServer
make all

# 修改httpdocs下文件的可读写权限
sudo chmod 666 httpdocs/post.html httpdocs/readme.html
# 修改cgi下脚本的可执行权限
sudo chmod 755 cgi/post.cgi

# 默认在8080端口运行httpd服务
echo "Usage: "
echo "1. ./webserver <IP> <Port>"
echo "2. ./webserver <Port>"
./webserver 8080

# 展示可访问的链接
echo "http://localhost:8080"
echo "http://localhost:8080/post.html"
