/* Native first-run/import backend. Exit 0 ready, 10 imported, 130 cancelled.
 * Only innoextract is executed; the Windows installer is always data. */
#include "sha256.h"
#include <algorithm>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
static const char *installer_name="setup_conflict_-_desert_storm_1.0_(21657).exe";
static const char *installer_sha="f840e10a7da55a71e89e5d83815c3f3e2bdec1206b20471291d87a3a02ea81aa";
static volatile sig_atomic_t cancelled;
static void on_signal(int){cancelled=1;}
struct Cancelled{};
static void check_cancel(){if(cancelled)throw Cancelled{};}
static void require(bool condition,const std::string &message){if(!condition)throw std::runtime_error(message);}
struct FD { int n; explicit FD(int fd):n(fd){} ~FD(){if(n>=0)close(n);} FD(const FD&)=delete; };
struct Entry {std::string sha,name;};
static std::string lower(std::string s){for(char &c:s)if(c>='A'&&c<='Z')c+=32;return s;}
static bool path_exists(const fs::path &p){struct stat s;return lstat(p.c_str(),&s)==0;}
static void sync_dir(const fs::path &p){FD fd(open(p.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));require(fd.n>=0,"Cannot open directory for sync");if(fsync(fd.n)&&errno!=EINVAL&&errno!=ENOTSUP)throw std::runtime_error("Directory sync failed");}
static std::string read_text(const fs::path &p,size_t limit=4*1024*1024){
 FD fd(open(p.c_str(),O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC));require(fd.n>=0,"Cannot read "+p.string());
 struct stat st{};require(!fstat(fd.n,&st)&&S_ISREG(st.st_mode)&&st.st_size>=0&&uint64_t(st.st_size)<=limit,"Invalid text file: "+p.string());
 std::string out;char b[8192];ssize_t n;while((n=read(fd.n,b,sizeof b))!=0){check_cancel();if(n<0){if(errno==EINTR)continue;throw std::runtime_error("Read failed");}out.append(b,size_t(n));require(out.size()<=limit,"Text file too large");}return out;
}
static void atomic_text(const fs::path &p,const std::string &text){
 std::string name=p.string()+".XXXXXX";std::vector<char> tmp(name.begin(),name.end());tmp.push_back(0);FD fd(mkstemp(tmp.data()));require(fd.n>=0,"Cannot create status/config file");
 try{size_t offset=0;while(offset<text.size()){ssize_t n=write(fd.n,text.data()+offset,text.size()-offset);if(n<0&&errno==EINTR)continue;require(n>0,"Write failed");offset+=size_t(n);}require(!fsync(fd.n),"File sync failed");require(!rename(tmp.data(),p.c_str()),"Atomic file update failed");sync_dir(p.parent_path());}catch(...){unlink(tmp.data());throw;}
}
static void status(const fs::path &root,int percent,std::string message){for(char &c:message)if(c=='\n'||c=='\r')c=' ';atomic_text(root/"install-status.txt",std::to_string(percent)+"\n"+message.substr(0,180)+"\n");}
static int safe_open(const fs::path &base,const std::string &name){
 // Open each component relative to a pinned directory, never following links.
 FD dir(open(base.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));require(dir.n>=0,"Missing/unsafe data directory");
 fs::path path(name);auto it=path.begin();for(;it!=path.end();){std::string part=it->string();++it;
  if(it==path.end()){int fd=openat(dir.n,part.c_str(),O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC);require(fd>=0,"Missing or unsafe game file: "+name);return fd;}
  int next=openat(dir.n,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);require(next>=0,"Missing or unsafe game directory: "+name);close(dir.n);dir.n=next;
 }throw std::runtime_error("Empty filename");
}
static std::string hash_fd(int fd,bool durable=false){
 struct stat st{};require(!fstat(fd,&st)&&S_ISREG(st.st_mode),"Expected a regular file");Sha256 sha;char block[65536];
 for(;;){check_cancel();ssize_t n=read(fd,block,sizeof block);if(n<0&&errno==EINTR)continue;require(n>=0,"File read failed");if(!n)break;sha.update(block,size_t(n));}
 if(durable)require(!fsync(fd),"Extracted file sync failed");
 return sha.finish();
}
static std::string digest(const fs::path &p){FD fd(open(p.c_str(),O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC));require(fd.n>=0,"Cannot read "+p.filename().string());return hash_fd(fd.n);}
static std::vector<Entry> manifest(const fs::path &p){
 std::vector<Entry> out;std::set<std::string> names;std::istringstream in(read_text(p));std::string line;
 while(std::getline(in,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();require(line.size()>66&&line.substr(64,2)=="  ","Invalid game manifest");Entry e{line.substr(0,64),line.substr(66)};
  require(e.sha.find_first_not_of("0123456789abcdef")==std::string::npos,"Invalid manifest checksum");
  require(!e.name.empty()&&!fs::path(e.name).is_absolute()&&e.name.find_first_of("\\\t\r\n") == std::string::npos&&e.name.find('\0')==std::string::npos,"Invalid manifest filename");
  require(e.name.front()!='-'&&e.name.back()!='/'&&e.name.find("//")==std::string::npos,"Invalid manifest filename");
  for(auto &part:fs::path(e.name))require(part!=".."&&part!=".","Invalid manifest path");
  require(names.insert(lower(e.name)).second,"Ambiguous game manifest");out.push_back(e);require(out.size()<=10000,"Manifest too large");
 }require(!out.empty(),"Empty game manifest");return out;
}
static std::string file_state(const fs::path &data,const std::vector<Entry> &entries,const std::string &manifest_hash){
 std::ostringstream out;out<<"RECONFLICT-NATIVE-READY-1\n"<<manifest_hash<<'\n';
 for(const auto &e:entries){check_cancel();FD fd(safe_open(data,e.name));struct stat s{};require(!fstat(fd.n,&s)&&S_ISREG(s.st_mode),"Invalid game file: "+e.name);
  out<<e.name<<'\t'<<s.st_size<<'\t'<<s.st_mtim.tv_sec<<':'<<s.st_mtim.tv_nsec<<'\t'<<s.st_ctim.tv_sec<<':'<<s.st_ctim.tv_nsec<<'\t'<<s.st_ino<<'\t'<<s.st_dev<<'\n';}
 return out.str();
}
static void validate(const fs::path &root,const fs::path &data,const std::vector<Entry> &entries,bool durable=false){
 for(size_t i=0;i<entries.size();i++){check_cancel();const auto &e=entries[i];status(root,int(i*100/entries.size()),"VERIFYING "+e.name);FD fd(safe_open(data,e.name));require(hash_fd(fd.n,durable)==e.sha,"Wrong version or damaged game file: "+e.name);}
}
static void ready(const fs::path &root,const std::vector<Entry>&entries,const std::string &mh){
 auto before=file_state(root/"data",entries,mh);bool cached=false;
 try{cached=read_text(root/"data-ready.native")==before;}catch(const std::runtime_error&){}
 if(cached)return;
 validate(root,root/"data",entries);require(file_state(root/"data",entries,mh)==before,"Game files changed during verification");atomic_text(root/"data-ready.native",before);
}
struct Stage { fs::path path; explicit Stage(const fs::path &root){std::string s=(root/".import-XXXXXX").string();std::vector<char>b(s.begin(),s.end());b.push_back(0);require(mkdtemp(b.data())!=nullptr,"Cannot create import directory");path=b.data();}~Stage(){std::error_code ec;if(!path.empty())fs::remove_all(path,ec);} };
struct Child {
 pid_t pid=-1;
 ~Child(){if(pid<=0)return;kill(-pid,SIGTERM);auto end=Clock::now()+std::chrono::seconds(5);int s;
  for(;;){pid_t r=waitpid(pid,&s,WNOHANG);if(r==pid||(r<0&&errno==ECHILD))return;if(Clock::now()>=end)break;std::this_thread::sleep_for(std::chrono::milliseconds(20));}
  kill(-pid,SIGKILL);while(waitpid(pid,&s,0)<0&&errno==EINTR){}
 }
};
static void extract(const fs::path &root,const fs::path &stage,const fs::path &extractor,const std::vector<Entry>&entries){
 require(fs::is_regular_file(extractor),"INNOEXTRACT MISSING. REINSTALL PORT PACKAGE.");
 std::vector<std::string> args{extractor.string(),"--extract","--exclude-temp","--color=0","--progress=0","--language","en-US","--output-dir",stage.string()};
 for(const auto&e:entries){args.push_back("--include");args.push_back(e.name);}args.push_back((root/installer_name).string());
 std::vector<char*> argv;for(auto &a:args)argv.push_back(a.data());argv.push_back(nullptr);
 Child child;child.pid=fork();require(child.pid>=0,"Cannot start extractor");
 if(child.pid==0){setpgid(0,0);signal(SIGINT,SIG_DFL);signal(SIGTERM,SIG_DFL);int log=open((root/"install.log").c_str(),O_WRONLY|O_APPEND|O_CREAT|O_NOFOLLOW,0644);if(log<0)_exit(126);dup2(log,1);dup2(log,2);close(log);
  auto libs=root/"libs.aarch64";if(!fs::is_directory(libs))libs=extractor.parent_path()/"lib";if(fs::is_directory(libs)){std::string value=libs.string();if(const char *old=getenv("LD_LIBRARY_PATH"))value+=":"+std::string(old);setenv("LD_LIBRARY_PATH",value.c_str(),1);}execv(extractor.c_str(),argv.data());_exit(127);}
 setpgid(child.pid,child.pid);
 for(;;){check_cancel();int state=0;pid_t r=waitpid(child.pid,&state,WNOHANG);if(r==child.pid){child.pid=-1;require(WIFEXITED(state)&&WEXITSTATUS(state)==0,"EXTRACTION FAILED. SEE INSTALL.LOG.");break;}if(r<0&&errno==EINTR)continue;require(r>=0,"Cannot wait for extractor");
  size_t count=0;for(const auto&e:entries){std::error_code ec;if(fs::is_regular_file(fs::symlink_status(stage/e.name,ec)))++count;}
  status(root,std::min(99,int(count*100/entries.size())),"EXTRACTING FILES "+std::to_string(count)+" OF "+std::to_string(entries.size()));
  for(int i=0;i<10;i++){check_cancel();std::this_thread::sleep_for(std::chrono::milliseconds(50));}
 }
}
static void sync_tree(const fs::path &root){
 std::vector<fs::path> dirs{root};
 for(const auto &entry:fs::recursive_directory_iterator(root)){
  check_cancel();if(fs::is_directory(entry.symlink_status()))dirs.push_back(entry.path());
 }
 for(auto it=dirs.rbegin();it!=dirs.rend();++it){check_cancel();sync_dir(*it);}
}
static void publish(Stage &stage,const fs::path &data){
 sync_tree(stage.path);check_cancel();require(!path_exists(data),"DATA FOLDER CHANGED. EXISTING FILES KEPT.");
 // Atomic no-replace publication on Linux; never overwrite a racing destination.
 require(syscall(SYS_renameat2,AT_FDCWD,stage.path.c_str(),AT_FDCWD,data.c_str(),1)==0,"Cannot publish import. EXISTING FILES KEPT.");stage.path.clear();sync_dir(data.parent_path());
}
static void free_space(const fs::path &root){struct statvfs s{};require(!statvfs(root.c_str(),&s),"Cannot check free space");uint64_t free=uint64_t(s.f_bavail)*s.f_frsize;
#ifdef DS_IMPORT_TEST
 if(const char *v=getenv("DS_TEST_FREE_BYTES"))free=std::stoull(v);
#endif
 require(free>=1100ull*1024*1024,"NEED AT LEAST 1.1 GIB FREE SPACE TO IMPORT.");}
static std::string expected_installer(){
#ifdef DS_IMPORT_TEST
 if(const char *v=getenv("DS_TEST_INSTALLER_SHA"))return v;
#endif
 return installer_sha;
}
static int prepare(const fs::path &root,const fs::path &extractor,const std::vector<Entry>&entries,const std::string &mh){
 auto data=root/"data";
 if(path_exists(data)){if(fs::is_directory(fs::symlink_status(data))&&fs::is_empty(data))fs::remove(data);else{ready(root,entries,mh);status(root,100,"GAME FILES READY");return 0;}}
 require(path_exists(root/installer_name),"COPY YOUR GOG INSTALLER HERE. SEE README.");status(root,-1,"CHECKING GOG INSTALLER");require(digest(root/installer_name)==expected_installer(),"UNSUPPORTED INSTALLER. NEED GOG 21657 ENGLISH.");free_space(root);
 Stage stage(root);auto started=Clock::now();extract(root,stage.path,extractor,entries);validate(root,stage.path,entries,true);publish(stage,data);
 atomic_text(root/"data-ready.native",file_state(data,entries,mh));struct rusage ru{};getrusage(RUSAGE_CHILDREN,&ru);std::ostringstream result;
 result<<"{\"seconds\":"<<std::chrono::duration<double>(Clock::now()-started).count()<<",\"extractor_peak_rss_kib\":"<<ru.ru_maxrss<<",\"files_verified\":"<<entries.size()<<",\"installer_sha256\":\""<<expected_installer()<<"\"}\n";
 atomic_text(root/"install-result.json",result.str());status(root,100,"IMPORT COMPLETE. INSTALLER STILL KEPT.");return 10;
}
static int remove_installer(const fs::path &root,const std::vector<Entry>&entries,const std::string &mh){
 ready(root,entries,mh);status(root,-1,"CHECKING INSTALLER BEFORE DELETION");struct stat before{},after{};require(!lstat((root/installer_name).c_str(),&before)&&S_ISREG(before.st_mode),"INSTALLER CHANGED. FILE KEPT.");
 require(digest(root/installer_name)==expected_installer(),"INSTALLER CHANGED. FILE KEPT.");check_cancel();require(!lstat((root/installer_name).c_str(),&after)&&S_ISREG(after.st_mode)&&before.st_ino==after.st_ino&&before.st_dev==after.st_dev&&before.st_size==after.st_size&&before.st_ctim.tv_sec==after.st_ctim.tv_sec&&before.st_ctim.tv_nsec==after.st_ctim.tv_nsec,"INSTALLER CHANGED. FILE KEPT.");
 require(!unlink((root/installer_name).c_str()),"Could not delete installer");sync_dir(root);status(root,100,"INSTALLER DELETED. GAME DATA KEPT.");return 0;
}
static int import_folder(const fs::path &root,const fs::path &source,const std::vector<Entry>&entries,const std::string &mh,bool check_only){
 std::map<std::string,std::vector<std::string>> files;
 require(fs::is_directory(source),"Source must be an installed game folder");
 for(auto &p:fs::recursive_directory_iterator(source)){check_cancel();if(p.is_regular_file())files[lower(p.path().lexically_relative(source).generic_string())].push_back(p.path().lexically_relative(source).generic_string());}
 std::vector<std::string> selected;
 for(const auto&e:entries){auto &matches=files[lower(e.name)];require(matches.size()==1,"Missing or ambiguous source file: "+e.name);FD fd(safe_open(source,matches[0]));status(root,-1,"VERIFYING SOURCE "+e.name);require(hash_fd(fd.n)==e.sha,"Wrong version or damaged source: "+e.name);selected.push_back(matches[0]);}
 if(check_only){status(root,100,"SOURCE FILES VERIFIED");return 0;}
 auto data=root/"data";if(path_exists(data)){require(fs::is_directory(fs::symlink_status(data))&&fs::is_empty(data),"Destination data must be absent or empty");fs::remove(data);}free_space(root);Stage stage(root);
 for(size_t i=0;i<entries.size();i++){check_cancel();auto target=stage.path/entries[i].name;fs::create_directories(target.parent_path());FD in(safe_open(source,selected[i]));FD out(open(target.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0644));require(out.n>=0,"Cannot copy game file");char b[65536];for(;;){check_cancel();ssize_t n=read(in.n,b,sizeof b);if(n<0&&errno==EINTR)continue;require(n>=0,"Source read failed");if(!n)break;ssize_t at=0;while(at<n){ssize_t k=write(out.n,b+at,size_t(n-at));if(k<0&&errno==EINTR)continue;require(k>0,"Copy failed");at+=k;}}require(!fsync(out.n),"Copy sync failed");}
 validate(root,stage.path,entries,true);publish(stage,data);atomic_text(root/"data-ready.native",file_state(data,entries,mh));status(root,100,"GAME DATA COPIED. SOURCE UNCHANGED.");return 0;
}
int main(int argc,char **argv){
 fs::path root,extractor,source,manifest_path;bool remove=false,check_only=false;
 try{char exe[4096];ssize_t n=readlink("/proc/self/exe",exe,sizeof exe-1);require(n>0&&n<ssize_t(sizeof exe-1),"Cannot locate helper");exe[n]=0;root=fs::path(exe).parent_path();
  for(int i=1;i<argc;i++){std::string a=argv[i];if(a=="--delete-installer")remove=true;else if(a=="--check-only")check_only=true;else if(a=="--help"){std::cout<<"prepare-game [--root DIR] [--delete-installer | --source DIR [--check-only]] [--extractor PATH] [--manifest PATH]\n";return 0;}else{require(i+1<argc,"Missing argument");fs::path v=argv[++i];if(a=="--root")root=v;else if(a=="--extractor")extractor=fs::absolute(v);else if(a=="--source")source=fs::canonical(v);else if(a=="--manifest")manifest_path=fs::absolute(v);else throw std::runtime_error("Unknown option: "+a);}}
  require(!(remove&&!source.empty())&&(!check_only||!source.empty()),"Invalid option combination");root=fs::canonical(root);require(fs::is_directory(root),"Invalid root directory");
  FD lock(open((root/".install.lock").c_str(),O_WRONLY|O_NONBLOCK|O_CREAT|O_NOFOLLOW|O_CLOEXEC,0644));require(lock.n>=0,"Cannot open import lock");struct stat lock_stat{};require(!fstat(lock.n,&lock_stat)&&S_ISREG(lock_stat.st_mode),"Invalid import lock");if(flock(lock.n,LOCK_EX|LOCK_NB)){std::cerr<<"Another import is active\n";return 1;}
  signal(SIGTERM,on_signal);signal(SIGINT,on_signal);
  try{if(manifest_path.empty())manifest_path=root/"game-files.sha256";auto entries=manifest(manifest_path);auto mh=digest(manifest_path);if(extractor.empty())extractor=root/"tools/innoextract";
   return remove?remove_installer(root,entries,mh):!source.empty()?import_folder(root,source,entries,mh,check_only):prepare(root,extractor,entries,mh);
  }catch(const Cancelled&){status(root,0,"CANCELLED. ORIGINAL INSTALLER KEPT.");return 130;}
  catch(const std::exception &e){status(root,0,e.what());std::ofstream log(root/"install.log",std::ios::app);log<<e.what()<<'\n';std::cerr<<e.what()<<'\n';return 1;}
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
