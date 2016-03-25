#include<cstdio>
#include<iostream>
//#include<omp.h>
#include<map>
#include<cstring>
#include<dirent.h>
#include<string>
#include<queue>
#include<sys/stat.h>
#include<cmath>
#include<cstdlib>
#include<unistd.h>
#include<hashlibpp.h>
#include<pthread.h>
#include<deque>
#include<bitset>
#include<iomanip>

using namespace std;

bool exact=true,direxact=false;
vector<string> common_tree;
bool check=true;
bool fcheck=false;
//map<string,int> filemap;
queue<string> directories;
queue<string> files;
map<string,int> fs;
string folder,file,prevfile;
int m,k;
bool* flag;
//map<int,set<int> > 
pthread_mutex_t critical;
pthread_barrier_t barrier;
vector<string> filesystem;
map<string, pair<int,bool> > filemap;
//map<int,set<int> > 
int count =0,fileCheck=0;
//vector<bool> common;
deque<string> common_dir; 
map<long long, int> sizeMap;
bool divergent=false;

/* This function is for checking whether a given entity is file or directory*/
bool isFile(string a){
  char b[1024];
  strncpy(b, a.c_str(), sizeof(b)-1);
  struct stat status;
  stat(b, &status);
  if(S_ISDIR(status.st_mode))
    return false;
  else
    return true;
}

/*This function is used for traversing over list of files in a directory in a filesystem*/
void getDirectory(vector<string> filesystem, string hash_path, queue<string> &directories, queue<string> &files, map<string,int> &fs, int m, int i){
  DIR* dir;
  dirent* entry;
  string a;
  char b[1024];
  strncpy(b, (filesystem[i]+hash_path).c_str(), sizeof(b));
  b[sizeof(b)-1]=0;
  string temp;
  if( (dir= opendir(b))!=NULL ){
    while( (entry=readdir(dir))!=NULL ){
      if( !(strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0) ){
        a = entry->d_name;
        temp = hash_path+"/"+a;
        pthread_mutex_lock(&critical);
        {
          fs[a]+=1;
        }
        if(fs[a]==m){
          {
            if(!isFile(filesystem[i]+hash_path+"/"+a)){
              directories.push(hash_path+"/"+a);
              common_dir.push_back(temp);
            }
            else{
              files.push(hash_path+"/"+a);
            }
            filemap[temp]=make_pair(count++,false);
          }
        }
        pthread_mutex_unlock(&critical);
      }
    }
    closedir(dir);
  }
}

/*This function returns the file pointer for the file which is opened*/
FILE* getFilePointer(vector<string> filesystem, string file, int i){
  char b[1024];
  string a;
  strncpy(b, (filesystem[i]+file).c_str(), sizeof(b));
  b[sizeof(b)-1]=0;
  FILE* fp;
  char buff[100];
  if((fp = fopen(b, "r"))!=NULL)
    return fp;
  else
    return NULL;
}

/*This function is for checking contents of a file in a filsystem and matching it with other filesystems*/
void checkFile2(FILE* fp, map<string, int> &fs, bool &flag, int m, int i){
  bool common=false;
  char buff[4096];
  string a;
  int n;
  if(fp==NULL){
    return;
  }
    if(flag==true && fp!=NULL){
      if(n = fread(buff, 1, 4095, fp)>0){
        flag=false;
      }
      if(flag){      
      a = buff;
      a = a.substr(0,n);
      #pragma omp critical
      pthread_mutex_lock(&critical);
      {
        fs[a]+=1;
        if(a.compare("")!=0 && fs[a]==m){
          check=true;
          common=true;
        }
        if(a.compare("")==0)
          flag=false;
      }
      pthread_mutex_unlock(&critical);
      }
      common=false;
    }
    else{
      a="";
    }
}  

/*This function is used for matching the size of a file in a filesystem with other filesystems*/
void checkSize(vector<string> filesystem, string file, int m, int i){
    struct stat stat_buf;
    int rc = stat((filesystem[i]+file).c_str(), &stat_buf);
    if(rc == 0){
      long long fsize = stat_buf.st_size;
      pthread_mutex_lock(&critical);
      {
        sizeMap[fsize]+=1;
        if(sizeMap[fsize]==m){
          check=true;
        }
      }
      pthread_mutex_unlock(&critical);
    }
}

/*This function is used for matching the hash of a file in a filesystem with other filesystems*/
void checkHash(FILE* fp, vector<string> filesystem, string file, map<string,int> &fs, int m, int i){
  if(fp==NULL)
    return;
  hashwrapper *myWrapper = new md5wrapper();
  string a=myWrapper->getHashFromFile(filesystem[i]+file);
  pthread_mutex_lock(&critical);
  {
  fs[a]+=1;
  if(fs[a]==m){
    check=true;
  }
  }
  pthread_mutex_unlock(&critical);
}

/*This function is for recursing over common directories in filesystems and to extract all the common pathnames in the filesystems*/
void *recurseDirectories(void* tid){
  int i = (long)tid;
    folder = "";
    while(!directories.empty()){
      pthread_barrier_wait(&barrier);
      if(!directories.empty())
      {
        if(i==0)
        {
          folder = directories.front();
          directories.pop();
        }
      }
      pthread_barrier_wait(&barrier);
      getDirectory(filesystem, folder, directories, files, fs, m, i);
      pthread_barrier_wait(&barrier);
      if(!fs.empty())
      {
        if(i==0)
        {
          if(exact){
            for(map<string,int>::iterator it = fs.begin();it!=fs.end();it++){
              if((*it).second==k)
                continue;
              else{
                exact=false;
              }
            }
          }
          fs.clear();
        }
      }
    }
}

/*This function is used for recursing over all common pathname files which are in majority*/
void* recurseFiles(void* tid){
  int i = (long)tid;
    FILE* fp;
    if(!files.empty())
    file = files.front();
    check=false;
    pthread_barrier_wait(&barrier);
    while(!files.empty()){
      if(i==0)
      {
        if(!fs.empty())
          fs.clear();
      }
      check=false;
      fileCheck=0;
      pthread_barrier_wait(&barrier);
      flag[i]=true;
      if(i==0)
      {
        files.pop();
      }
      fp=getFilePointer(filesystem, file, i);
      checkSize(filesystem, file, m, i);
      pthread_barrier_wait(&barrier);
      if(i==0)
      {
        if(!sizeMap.empty())
        sizeMap.clear();
      }
      if(check){
      pthread_barrier_wait(&barrier);
      check=false;
      pthread_barrier_wait(&barrier);
      checkHash(fp,filesystem, file, fs, m, i);
      pthread_barrier_wait(&barrier);
      if(i==0)
      {
      prevfile = file;
      if(!files.empty()){
      file = files.front();
      }        
      }
      pthread_barrier_wait(&barrier);
      if(check!=false){
      while(check==true){
        if(i==0)
        {
          if(!fs.empty())
            fs.clear();
        }
        pthread_barrier_wait(&barrier);
        check=false;
        pthread_barrier_wait(&barrier);
        checkFile2(fp, fs, flag[i], m, i);
        pthread_barrier_wait(&barrier);
      } 
        pthread_mutex_lock(&critical);
        {
          if(!flag[i])
            fileCheck++;
          if(fileCheck==m)
            check=false;
        }
        pthread_mutex_unlock(&critical);
        pthread_barrier_wait(&barrier);
        if(exact){
          if(fileCheck!=k){
            exact=false;
          }
        }
        if(!check)
          fcheck=true;
        if(fp!=NULL){
        fclose(fp);
        }

      pthread_barrier_wait(&barrier);
      if(fcheck==true){
        if(i==0)
        {
          common_tree.push_back(prevfile);
          filemap[prevfile].second=true;
        }
      }
      }
      else{
        if(fp!=NULL)
          fclose(fp);
        exact=false;
      }
      }
      else{
        if(fp!=NULL)
          fclose(fp);
        exact=false;
      }
    }
}

/*This function is used for matching the contents of a directory in a filesystem with other filesystems and for checking whether directory is in common majority or not*/
void checkDirectory(vector<string> filesystem, string hash_path, queue<string> &directories, queue<string> &files, map<string,int> &fs, int m, int i){
  DIR* dir;
  dirent* entry;
  string a;
  char b[1024];
  strncpy(b, (filesystem[i]+hash_path).c_str(), sizeof(b));
  b[sizeof(b)-1]=0;
  string temp;
  vector<char> contents(count,'0');
  bool uncommon=false;
  if( (dir= opendir(b))!=NULL ){
    while( (entry=readdir(dir))!=NULL ){
      if( !(strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0) ){
        a = entry->d_name;
        temp = hash_path+"/"+a;
        if(hash_path.compare("/Data1")==0){
          cout<<boolalpha;
        }
        if(filemap.find(temp)==filemap.end())
          uncommon=true;
        else if(!filemap[temp].second)
          uncommon=true;
        if(!uncommon){
          contents[filemap[temp].first]='1';
        }
      }
    }
    if(!uncommon){
      a=string(contents.begin(),contents.end());
    pthread_mutex_lock(&critical);
    {
      {
        fs[a]+=1;
      }
      if(fs[a]==m)
      {
        common_tree.push_back(hash_path);
        filemap[hash_path].second = true;
      }
      if(exact){
        if(fs[a]==k)
          direxact=true;
      }
    }
    pthread_mutex_unlock(&critical);
    }  
    closedir(dir);
  }
}

/*This function is for checking which all directories out of common pathnames are in majority in all filesystems*/
void* checkSubtrees(void* tid){
    int i = (long)tid;
    while(!common_dir.empty()){
      pthread_barrier_wait(&barrier);
      if(!common_dir.empty())
      {
        if(i==0)
        {
          folder = common_dir.back();
          common_dir.pop_back();
        }
      }
      direxact=false;
      pthread_barrier_wait(&barrier);
      checkDirectory(filesystem, folder, directories, files, fs, m, i);
      pthread_barrier_wait(&barrier);
      if(exact){
        if(!direxact){
          exact=false;
        }
      }
      if(!fs.empty())
      {
        if(i==0)
        {
          fs.clear();
        }
      }
    }
}
  
void compareFS(vector<string> filesystem, int k1){
  string str = "";
  directories.push(str);
  folder=str;
  flag = new bool[5];
  k=k1;
  m = ceil(k/2.0);
  if(k==2)
    m=2;
  count=0;
  common_dir.push_back("");
  pthread_t thread[k];
  void* status;
  pthread_barrier_init(&barrier, NULL, k);
  pthread_mutex_init(&critical, NULL);
  for(long i=0;i<k;i++){
    int rc = pthread_create(&thread[i], NULL, recurseDirectories, (void *) i);
    if(rc){
      cout<<"Error in retur code while creation"<<i<<endl;
      exit(-1);
    }
  }
  for(int i=0;i<k;i++){
    int rc = pthread_join(thread[i], &status);
    if(rc){
      cout<<"Error in return code while joining"<<i<<endl;
      exit(-1);
    }
  }
  for(long i=0;i<k;i++){
    int rc = pthread_create(&thread[i], NULL, recurseFiles, (void *) i);
    if(rc){
      cout<<"Error in retur code while creation"<<i<<endl;
      exit(-1);
    }
  }
  for(int i=0;i<k;i++){
    int rc = pthread_join(thread[i], &status);
    if(rc){
      cout<<"Error in return code while joining"<<i<<endl;
      exit(-1);
    }
  }
  folder = "";
  for(long i=0;i<k;i++){
    int rc = pthread_create(&thread[i], NULL, checkSubtrees, (void *) i);
    if(rc){
      cout<<"Error in retur code while creation"<<i<<endl;
      exit(-1);
    }
  }
  for(int i=0;i<k;i++){
    int rc = pthread_join(thread[i], &status);
    if(rc){
      cout<<"Error in return code while joining"<<i<<endl;
      exit(-1);
    }
  }
  if(common_tree.empty())
    divergent=true;
  cout<<setbase(10);
  long long n=common_tree.size();
  cout<<"COMMON_TREE"<<endl;
  for(long long i=0;i<n;i++){
    cout<<common_tree[i]<<endl;
  }
}     
  
int main(int argc, char* argv[]){
  char buf[1024];
  getcwd(buf,1024);
  string cwd = buf;
  cout<<cwd<<endl;
  string a="";
  int n=atoi(argv[1]);
  for(int i=0;i<n;i++){
    a = argv[i+2];
    filesystem.push_back(a);
  } 
  compareFS(filesystem, n);
  if(exact)
    cout<<"FILESYSTEMS ARE EXACTLY SAME"<<endl;
  if(divergent)
    cout<<"FILESYSTEMS ARE DIVERGENT"<<endl;
}
