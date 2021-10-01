#!/usr/bin/python
# -*- coding: UTF-8 -*-


import sys, os

length = os.getenv('CONTENT_LENGTH')
print(length);

if length:
    postdata = sys.stdin.read(int(length))
    print("Content-type:text/html\n")
    print('<html lang="en">')
    print('<head>')
    print('<meta charset="UTF-8">')
    print('<title>注册信息</title>')
    print('</head>')
    print('<body>')
    print('<h2> Your POST data: </h2>')
    print('<ul>')
    for data in postdata.split('&'):
        if "usrfile" in data:
            #     <a href="../Makefile" download="Makefile">file</a>
            # /home/bright/GitRepo/MyTinyWebServer
            print('<li> <a href="../files/' + data.split('=')[1] + '" download="' + data.split('=')[1] + '">' + data + '</a></li>')
        else:
            print('<li>' + data + '</li>')
    print('</ul>')
    print('</body>')
    print('</html>')

else:
    print("Content-type:text/html\n")
    print('no found')