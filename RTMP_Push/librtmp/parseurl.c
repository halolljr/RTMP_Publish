/*
 *  Copyright (C) 2009 Andrej Stepanchuk
 *  Copyright (C) 2009-2010 Howard Chu
 *
 *  This file is part of librtmp.
 *
 *  librtmp is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2.1,
 *  or (at your option) any later version.
 *
 *  librtmp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with librtmp see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *  http://www.gnu.org/copyleft/lgpl.html
 */

#include <stdlib.h>
#include <string.h>

#include <assert.h>
#include <ctype.h>

#include "rtmp_sys.h"
#include "log.h"

int RTMP_ParseURL(const char *url, int *protocol, AVal *host, unsigned int *port,
	AVal *playpath, AVal *app)
{
	//rtmp://localhost:1935/vod/mp4:sample1_1500kbps.f4v
	char *p, *end, *col, *ques, *slash;

	RTMP_LogInfo(RTMP_LOGDEBUG, "Parsing...");

	*protocol = RTMP_PROTOCOL_RTMP;
	*port = 0;
	playpath->av_len = 0;
	playpath->av_val = NULL;
	app->av_len = 0;
	app->av_val = NULL;

    /* 字符串解析 */
    /* 查找“://”  */
    //函数原型：char *strstr(char *str1, char *str2);
    //功能：找出str2字符串在str1字符串中第一次出现的位置（不包括str2的串结束符）。
    //返回值：返回该位置的指针，如找不到，返回空指针。
	/* look for usual :// pattern */
	p = strstr(url, "://");
	if(!p) {
		RTMP_LogInfo(RTMP_LOGERROR, "RTMP URL: No :// in url!");
		return FALSE;
	}
	{
		//指针相减，返回“://”之前字符串长度len
		int len = (int)(p-url);
		//获取使用的协议
		//strncasecmp：比较字符串url开始的前4个字符是否和""一样，不区分大小写
		if(len == 4 && strncasecmp(url, "rtmp", 4)==0)
			*protocol = RTMP_PROTOCOL_RTMP;
		else if(len == 5 && strncasecmp(url, "rtmpt", 5)==0)
			*protocol = RTMP_PROTOCOL_RTMPT;
		else if(len == 5 && strncasecmp(url, "rtmps", 5)==0)
				*protocol = RTMP_PROTOCOL_RTMPS;
		else if(len == 5 && strncasecmp(url, "rtmpe", 5)==0)
				*protocol = RTMP_PROTOCOL_RTMPE;
		else if(len == 5 && strncasecmp(url, "rtmfp", 5)==0)
				*protocol = RTMP_PROTOCOL_RTMFP;
		else if(len == 6 && strncasecmp(url, "rtmpte", 6)==0)
				*protocol = RTMP_PROTOCOL_RTMPTE;
		else if(len == 6 && strncasecmp(url, "rtmpts", 6)==0)
				*protocol = RTMP_PROTOCOL_RTMPTS;
		else {
			RTMP_LogInfo(RTMP_LOGWARNING, "Unknown protocol!\n");
			goto parsehost;
		}
	}

	RTMP_LogInfo(RTMP_LOGDEBUG, "Parsed protocol: %d", *protocol);

parsehost:
    //获取主机名称，跳过"://"
	// p指向 "localhost:1935/vod/mp4:sample1_1500kbps.f4v"
	p+=3;
    /* 检查一下主机名 */
	if(*p==0) {
		RTMP_LogInfo(RTMP_LOGWARNING, "No hostname in URL!");
		return FALSE;
	}
    //原型：char *strchr(const char *s,char c);
    //功能：查找字符串s中首次出现字符c的位置
    //说明：返回首次出现c的位置的指针，如果s中不存在c则返回NULL。
	
	//localhost:1935/vod/mp4:sample1_1500kbps.f4v
	end   = p + strlen(p);	//指向结尾的指针
	col   = strchr(p, ':');	//指向冒号（第一个）的指针
	ques  = strchr(p, '?');	//指向问号（第一个）的指针
	slash = strchr(p, '/');	//指向斜杠（第一个）的指针
	//确定主机名的长度
	{
		int hostlen;
		if (slash)
			hostlen = slash - p;	// "localhost:1935"
		else
			hostlen = end - p;
		if (col && col - p < hostlen)
			hostlen = col - p;	//"localhost"

		if (hostlen < 256) {
			host->av_val = p;
			host->av_len = hostlen;
			RTMP_LogInfo(RTMP_LOGDEBUG, "Parsed host    : %.*s", hostlen, host->av_val);
		}
		else {
			RTMP_LogInfo(RTMP_LOGWARNING, "Hostname exceeds 255 characters!");
		}
		p += hostlen;
		//此时p=":1935/vod/mp4:sample1_1500kbps.f4v"
	}
    /* 获取端口号 */
	if(*p == ':') {
		unsigned int p2;
		p++;
		//对于 ":1935/vod/mp4:sample1_1500kbps.f4v"，atoi 只会读取前面的 "1935" 并将其转换成整数 1935，赋值给变量 p2。
		p2 = atoi(p);
		if(p2 > 65535) {
			RTMP_LogInfo(RTMP_LOGWARNING, "Invalid port number!");
		} else {
			*port = p2;
		}
	}

	if(!slash) {
		RTMP_LogInfo(RTMP_LOGWARNING, "No application or playpath in URL!");
		return TRUE;
	}
	p = slash+1;
	//此时p="vod/mp4:sample1_1500kbps.f4v"
	{
		/* 获取应用程序(application)
		 * rtmp://host[:port]/app[/appinstance][/...]
		 * application = app[/appinstance]
		 */
		//用于查找 p 中后续出现的斜杠，用以区分不同层级的路径（例如应用名、应用实例等）。
		char *slash2, *slash3 = NULL, *slash4 = NULL;
		//分别用于记录应用部分的总长度与实际要提取的长度。
		int applen, appnamelen;

		slash2 = strchr(p, '/');//指向第二个斜杠
		if(slash2)
			slash3 = strchr(slash2+1, '/');	//指向第三个斜杠，注意slash2之所以+1是因为让其后移一位
		if(slash3)
			slash4 = strchr(slash3+1, '/');//指向第四个斜杠，注意slash3之所以+1是因为让其后移一位

		applen = end-p; /* ondemand, pass all parameters as app */
		appnamelen = applen; /* ondemand length */

		//特殊情况 1：URL 中包含 "slist=" 参数，ques 是之前通过 strchr(url, '?') 获取到的指向问号的位置。
		if(ques && strstr(p, "slist=")) { /* whatever it is, the '?' and slist= means we need to use everything as app and parse plapath from slist= */
			//所有问号后面的内容不作为应用名称，而另作处理。
			appnamelen = ques-p;
		}
		//特殊情况 2：“ondemand / ”前缀
		else if(strncmp(p, "ondemand/", 9)==0) {
					/* app = ondemand/foobar, only pass app=ondemand */
					applen = 8;
					appnamelen = 8;
		}
		else { /* app!=ondemand, so app is app[/appinstance] */
			if(slash4)
				appnamelen = slash4-p;
			else if(slash3)
				appnamelen = slash3-p;
			else if(slash2)
				appnamelen = slash2-p;

			applen = appnamelen;
		}

		app->av_val = p;
		app->av_len = applen;
		RTMP_LogInfo(RTMP_LOGDEBUG, "Parsed app     : %.*s", applen, p);

		p += appnamelen;
	}

	if (*p == '/')
		p++;

	if (end-p) {
		AVal av = {p, end-p};
		RTMP_ParsePlaypath(&av, playpath);
	}

	return TRUE;
}


/*
 * 从URL中获取播放路径（playpath）。播放路径是URL中“rtmp://host:port/app/”后面的部分
 *
 * 获取FMS能够识别的播放路径
 * mp4 流: 前面添加 "mp4:", 删除扩展名
 * mp3 流: 前面添加 "mp3:", 删除扩展名
 * flv 流: 删除扩展名
 */
void RTMP_ParsePlaypath(AVal *in, AVal *out) {
	int addMP4 = 0;
	int addMP3 = 0;
	int subExt = 0;
	const char *playpath = in->av_val;
	const char *temp, *q, *ext = NULL;
	const char *ppstart = playpath;
	char *streamname, *destptr, *p;

	int pplen = in->av_len;

	out->av_val = NULL;
	out->av_len = 0;

	if ((*ppstart == '?') &&
	    (temp=strstr(ppstart, "slist=")) != 0) {
		ppstart = temp+6;
		pplen = strlen(ppstart);

		temp = strchr(ppstart, '&');
		if (temp) {
			pplen = temp-ppstart;
		}
	}

	q = strchr(ppstart, '?');
	if (pplen >= 4) {
		if (q)
			ext = q-4;
		else
			ext = &ppstart[pplen-4];
		if ((strncmp(ext, ".f4v", 4) == 0) ||
		    (strncmp(ext, ".mp4", 4) == 0)) {
			addMP4 = 1;
			subExt = 1;
		/* Only remove .flv from rtmp URL, not slist params */
		} else if ((ppstart == playpath) &&
		    (strncmp(ext, ".flv", 4) == 0)) {
			subExt = 1;
		} else if (strncmp(ext, ".mp3", 4) == 0) {
			addMP3 = 1;
			subExt = 1;
		}
	}

	streamname = (char *)malloc((pplen+4+1)*sizeof(char));
	if (!streamname)
		return;

	destptr = streamname;
	if (addMP4) {
		if (strncmp(ppstart, "mp4:", 4)) {
			strcpy(destptr, "mp4:");
			destptr += 4;
		} else {
			subExt = 0;
		}
	} else if (addMP3) {
		if (strncmp(ppstart, "mp3:", 4)) {
			strcpy(destptr, "mp3:");
			destptr += 4;
		} else {
			subExt = 0;
		}
	}

 	for (p=(char *)ppstart; pplen >0;) {
		/* skip extension */
		if (subExt && p == ext) {
			p += 4;
			pplen -= 4;
			continue;
		}
		if (*p == '%') {
			unsigned int c;
			sscanf(p+1, "%02x", &c);
			*destptr++ = c;
			pplen -= 3;
			p += 3;
		} else {
			*destptr++ = *p++;
			pplen--;
		}
	}
	*destptr = '\0';

	out->av_val = streamname;
	out->av_len = destptr - streamname;
}
