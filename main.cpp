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

long dwLen=0;
long lastLen=0;
FileInfo cFiles[THREADS_NUMS+1];

double  getFileLength(char * pUrl)
{
    CURL * pCurl=curl_easy_init();
    if(NULL==pCurl)
    {
        cout<<"curl_easy_init error!"<<endl;
        return false;
    }

    curl_easy_setopt(pCurl,CURLOPT_URL,pUrl);
    curl_easy_setopt(pCurl,CURLOPT_HEADER ,1);
    curl_easy_setopt(pCurl,CURLOPT_NOBODY ,1);

    CURLcode tRet=curl_easy_perform(pCurl);

    if(0!=tRet)
    {
        cout<<"curl_easy_perform error"<<endl;
        return false;
    }

    double  duLd=0;
    tRet=curl_easy_getinfo(pCurl,CURLINFO_CONTENT_LENGTH_DOWNLOAD,&duLd);

    curl_easy_cleanup(pCurl);

    return duLd;
}

size_t  writeFile(void *pData, size_t dwSize, size_t dwMemb, void * pFile)
{
    FileInfo * pFileInfo=(FileInfo *)pFile;
    cout<<"id: "<<pFileInfo->tid<<" offset: "<<pFileInfo->offset<<endl;

    memcpy((char *)pFileInfo->pFile+pFileInfo->offset,(char *)pData,dwSize*dwMemb);

    pFileInfo->offset+=dwSize*dwMemb;
    pFileInfo->used+=dwSize*dwMemb;

    return dwSize*dwMemb;


}

int progress_callback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow){
	if (dltotal != 0)
	{
		//printf("%lf / %lf (%lf %%)\n", dlnow, dltotal, dlnow*100.0 / dltotal);
		long totalUsedLen=0;
		//long totalLen=0;
		for(int i=0;i<THREADS_NUMS+1;i++)
		{
            totalUsedLen+=cFiles[i].used;
            totalUsedLen+=cFiles[i].totalLen;
		}


		printf("%ld / %ld (%ld %%)\n",totalUsedLen,dwLen,totalUsedLen*100/dwLen);
	}
	return 0;
}


void *  works(void * arg)
{
    FileInfo  * pFile=(FileInfo *)arg;

    CURL * pCurl=curl_easy_init();
    if(NULL==pCurl)
    {
        cout<<"curl_easy_init error!"<<endl;
        return NULL;
    }

    if(pFile->file)
    {
        cout<<"hello"<<endl;
        fscanf(pFile->file,"%ld-%ld-%ld",&pFile->offset,&pFile->endpos,&pFile->totalLen);
    }


    if(pFile->offset>=pFile->endpos-1)
    {
        cout<<pFile->tid<<"  already downed: "<<pFile->offset<<"--"<<pFile->endpos<<endl;
        return NULL;
    }

    char buffer[64]={0};
    snprintf(buffer,64,"%ld-%ld",pFile->offset,pFile->endpos);

    cout<<"offset/endpos: "<<pFile->offset<<"--"<<pFile->endpos<<endl;

    //cout<<cFile.tid<<": "<<cFile.offset<<" -- "<<cFile.endpos<<endl;

    curl_easy_setopt(pCurl,CURLOPT_URL,pFile->pUrl);
    curl_easy_setopt(pCurl,CURLOPT_WRITEFUNCTION,writeFile);
    curl_easy_setopt(pCurl,CURLOPT_WRITEDATA ,pFile);
    curl_easy_setopt(pCurl,CURLOPT_RANGE,buffer);
    curl_easy_setopt(pCurl,CURLOPT_NOPROGRESS ,0L);
    curl_easy_setopt(pCurl,CURLOPT_PROGRESSFUNCTION,progress_callback);
    curl_easy_setopt(pCurl,CURLOPT_PROGRESSDATA,pFile);


    CURLcode tRet=curl_easy_perform(pCurl);

    if(0!=tRet)
    {
        cout<<"curl_easy_perform error"<<endl;
        return NULL;
    }

    curl_easy_cleanup(pCurl);

    return NULL;
}

bool downFile(char * pUrl, char * pName)
{
    dwLen=(long)getFileLength(pUrl);

    cout<<"dwLen: "<<dwLen<<endl;

    int fd=open(pName,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR);

    if(fd==-1)
    {
        cout<<"open failed"<<endl;
        return false;
    }

    if(lseek(fd,dwLen,SEEK_SET)==-1)
    {
        cout<<"lseek failed"<<endl;
        close(fd);
        return false;
    }

    if(write(fd,"",1)!=1)
    {
        cout<<"write failed"<<endl;
        close(fd);
        return false;
    }

    char * filePos=(char *)mmap(NULL,dwLen,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);

    if(filePos==MAP_FAILED)
    {
        close(fd);

        cout<<"mmap failed: "<<errno<<endl;
        return false;
    }

    int slice=dwLen/THREADS_NUMS;
    FILE * file=fopen("downTemp.txt","r+");

    //10 threads
    for(int i=0;i<THREADS_NUMS+1;i++)
    {
        cFiles[i].offset=i*slice;
        cFiles[i].pUrl=pUrl;
        cFiles[i].pFile=filePos;
        //cFiles[i].used=0;
        cFiles[i].file=file;
        if(i==THREADS_NUMS)
        {
            cFiles[i].endpos=dwLen-1;
            //cFiles[i].totalLen=cFiles[i].endpos-cFiles[i].offset+1;
        }
        else
        {
            cFiles[i].endpos=(i+1)*slice-1;
            //cFiles[i].totalLen=slice;
        }
        pthread_create(&cFiles[i].tid,NULL,works,&cFiles[i]);
        usleep(1);
    }

    for(int i=0;i<THREADS_NUMS+1;i++)
    {
        pthread_join(cFiles[i].tid,NULL);
    }

    close(fd);

    munmap(filePos,dwLen);


    return true;
}

void sighandler_func(int arg)
{
    cout<<"arg: "<<arg<<endl;
    int fd=open("downTemp.txt",O_RDWR|O_CREAT,S_IRUSR|S_IWUSR);
    for(int i=0;i<THREADS_NUMS+1;i++)
    {
        cFiles[i].totalLen=cFiles[i].used;
        //cout<<"used: "<<cFiles[i].used<<"/"<<cFiles[i].totalLen<<endl;
        char buffer[64]={0};
        snprintf(buffer,64,"%ld-%ld-%ld\n",cFiles[i].offset,cFiles[i].endpos,cFiles[i].totalLen);
        write(fd,buffer,strlen(buffer));
    }

    close(fd);

    exit(-1);
}

int main(int argc, char ** argv)
{
    if(SIG_ERR==signal(SIGINT,sighandler_func))
    {
        cout<<"signal error"<<endl;
        return 0;
    }
    downFile(argv[1],argv[2]);
    return 0;
}
