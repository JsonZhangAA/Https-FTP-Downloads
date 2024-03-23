#pragma once

#include <iostream>
#include <fstream>
#include <curl/curl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

#define  THREADS_NUMS  (10)

class  FileInfo
{
public:
    void * pFile;
    size_t offset;
    size_t endpos;
    char * pUrl;
    pthread_t  tid;
    size_t  used;
    FILE * file;
    size_t totalLen;
};

size_t  writeFile(void *pData, size_t dwSize, size_t dwMemb, void * pFile);
int progress_callback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow);
void *  works(void * arg);
void sighandler_func(int arg);

class DownFile
{
    public:
        virtual bool downFile()=0;
};

class  HttpDownFile:public DownFile
{
public:
    HttpDownFile(char * pUrl, char * pFile):m_pUrl(pUrl),m_pFile(pFile){
        if(SIG_ERR==signal(SIGINT,sighandler_func))
        {
            cout<<"signal error"<<endl;
        }
    }

    virtual bool downFile();

private:
    double  getFileLength();

    long lastLen=0;
    char * m_pUrl;  //需要下载的http连接
    char * m_pFile; //本地存储文件的位置
};
