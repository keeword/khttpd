#!/usr/bin/python2.7
#-*- coding=utf-8 -*-

import cgi
import os

param = cgi.parse_qs(os.getenv("QUERY_STRING"))
print(str(param['text']))
print(str(param['number']))
