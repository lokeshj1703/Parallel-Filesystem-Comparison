#include<cstdio>
#include<iostream>
#include<omp.h>
#include<map>
#include<cstring>
#include<dirent.h>
#include<string>
#include<queue>
#include<deque>
#include<sys/stat.h>
#include<cmath>
#include<cstdlib>
#include<unistd.h>
#include<hashlibpp.h>
#include<bitset>
#include<iomanip>

using namespace std;

bool exact=true,direxact=false;
vector<string> common_tree;
bool check=true;
bool fcheck=false;
map<string, pair<int,bool> > filemap;
//map<int,set<int> > 
int count =0,fileCheck=0;
//vector<bool> common;
deque<string> common_dir; 
map<long long, int> sizeMap;
int k;
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
    //directories.pop();
    while( (entry=readdir(dir))!=NULL ){
      if( !(strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0) ){
        a = entry->d_name;
        temp = hash_path+"/"+a;
        #pragma omp critical
        {
          fs[a]+=1;
        if(fs[a]==m){
          //#pragma omp critical
          {
            if(!isFile(filesystem[i]+temp)){
              directories.push(temp);
              //common_tree.push_back(temp);  
              common_dir.push_back(temp);
            }
            else{
              files.push(temp);
            }
            filemap[temp]=make_pair(count++,false);
          }
        }
        }
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
  char buff[1024];
  string a;
  int n;
  if(fp==NULL){
    return;
  }
    if(flag==true && fp!=NULL){
      if((n=fread(buff, 1, 1023, fp)>0)){
        flag=false;
      }
      if(flag){      
      a = buff;
      a = a.substr(0,n);
      #pragma omp critical
      {
        fs[a]+=1;
        if(a.compare("")!=0 && fs[a]==m){
          check=true;
          common=true;
        }
        if(a.compare("")==0)
          flag=false;
      }
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
      #pragma omp critical
      {
        sizeMap[fsize]+=1;
        if(sizeMap[fsize]==m){
          check=true;
        }
      }
    }
}
  
/*This function is used for matching the hash of a file in a filesystem with other filesystems*/
void checkHash(FILE* fp, vector<string> filesystem, string file, map<string,int> &fs, int m, int i){
  if(fp==NULL)
    return;
  hashwrapper *myWrapper = new md5wrapper();
  string a=myWrapper->getHashFromFile(filesystem[i]+file);
  #pragma omp critical
  {
  fs[a]+=1;
  if(fs[a]==m){
    check=true;
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
    #pragma omp critical
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
    }  
    closedir(dir);
  }
}

void compareFS(vector<string> filesystem, int k1){
  string str = "";
  count=0;
  queue<string> directories;
  queue<string> files;
  map<string,int> fs;
  string folder,file,prevfile;
  k=k1;
  int m = ceil(k/2.0);
  if(k==2)
    m=2;
  int i=0;
  directories.push(str);
  folder=str;
  bool flag[k];
  common_dir.push_back("");
  #pragma omp parallel shared(directories, files, fs, folder, file, m, str, flag, prevfile) private(i) num_threads(k)
  {
    i = omp_get_thread_num();
/*This function is for recursing over common directories in filesystems and to extract all the common pathnames in the filesystems*/
    while(!directories.empty()){
      #pragma omp barrier
      if(!directories.empty())
      {
        #pragma omp master
        {
          folder = directories.front();
          directories.pop();
        }
      }
      #pragma omp barrier
      getDirectory(filesystem, folder, directories, files, fs, m, i);
      #pragma omp barrier
      if(!fs.empty())
      {
        #pragma omp master
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
        //#pragma omp barrier
      }
    }
    FILE* fp;
    if(!files.empty())
    file = files.front();
    check=false;

    //#pragma omp barrier
/*This loop is used for recursing over all common pathname files to check which files are in majority*/
    while(!files.empty()){
      //#pragma omp barrier
      #pragma omp barrier
      #pragma omp master
      {
        if(!fs.empty())
          fs.clear();
        file = files.front();
        files.pop();
      }
      check=false;
      fileCheck=0;
      #pragma omp barrier
      flag[i]=true;
      fp=getFilePointer(filesystem, file, i);
      checkSize(filesystem, file, m, i);
      #pragma omp barrier
      #pragma omp master
      {
        if(!sizeMap.empty())
        sizeMap.clear();
      }
      if(check){
      #pragma omp barrier
      check=false;
      #pragma omp barrier
      checkHash(fp,filesystem, file, fs, m, i);
      #pragma omp barrier
      /*#pragma omp master
      {
      prevfile = file;
      if(!files.empty()){
      }        
      }*/
      //#pragma omp barrier
      if(check!=false){
      while(check==true){
        #pragma omp master
        {
          if(!fs.empty())
            fs.clear();
        }
        #pragma omp barrier
        check=false;
        #pragma omp barrier
        checkFile2(fp, fs, flag[i], m, i);
        #pragma omp barrier
      } 
      //#pragma omp barrier
        #pragma omp critical
        {
          if(!flag[i])
            fileCheck++;
          if(fileCheck==m)
            check=false;
        }
        #pragma omp barrier
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

      //check=false;
      #pragma omp barrier
      if(fcheck==true){
        #pragma omp master
        {
          common_tree.push_back(file);
          filemap[file].second=true;
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
    #pragma omp master
    {
      if(!common_dir.empty())
      folder = common_dir.back();
    }
/*This loop is for checking which all directories out of common pathnames are in majority in all filesystems*/
    #pragma omp barrier
    while(!common_dir.empty()){
      #pragma omp barrier
      if(!common_dir.empty())
      {
        #pragma omp master
        {
          folder = common_dir.back();
          common_dir.pop_back();
        }
      }
      #pragma omp barrier
      checkDirectory(filesystem, folder, directories, files, fs, m, i);
      #pragma omp barrier
      if(exact){
        if(!direxact){
          exact=false;
        }
      }
      //#pragma omp master
      //{
      //if(!common_dir.empty())
      //}
      //#pragma omp barrier
      if(!fs.empty())
      {
        #pragma omp master
        {
          fs.clear();
        }
        //#pragma omp barrier
      }
    }

  }
  cout<<setbase(10);
  if(common_tree.empty())
    divergent=true;
  cout<<"COMMON_TREE"<<endl;
  long long n=common_tree.size();
  for(long long i=0;i<n;i++){
    cout<<common_tree[i]<<endl;
  }
}     
  
int main(int argc, char* argv[]){
  vector<string> filesystem;
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
