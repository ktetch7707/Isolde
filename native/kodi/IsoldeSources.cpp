// SPDX-License-Identifier: GPL-2.0-or-later
// Compiled inside Kodi, not loaded into Isolde. Kodi owns every library mutation.
#include "IsoldeSources.h"
#include "ServiceBroker.h"
#include "addons/AddonManager.h"
#include "addons/Scraper.h"
#include "filesystem/SpecialProtocol.h"
#include "settings/MediaSourceSettings.h"
#include "utils/JSONVariantParser.h"
#include "utils/JSONVariantWriter.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "video/VideoDatabase.h"
#include "video/VideoInfoScanner.h"
#include "video/VideoLibraryQueue.h"
#include "music/MusicLibraryQueue.h"
#include <algorithm>
#include <cstdlib>
#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
struct Descriptor
{
  int value;
  explicit Descriptor(int fd) : value(fd) { if(fd < 0) throw std::runtime_error("Managed source storage unavailable"); }
  ~Descriptor() { close(value); }
  Descriptor(const Descriptor&) = delete;
};
bool Token(const std::string& token)
{
  return token.size()==32 && token.find_first_not_of("0123456789abcdef")==std::string::npos;
}
std::filesystem::path SourceFile()
{
  const auto file=std::filesystem::path(CSpecialProtocol::TranslatePath(CMediaSourceSettings::GetSourcesFile()));
  const auto root=std::filesystem::canonical(CSpecialProtocol::TranslatePath("special://masterprofile"));
  const auto parent=std::filesystem::canonical(file.parent_path());
  const auto relative=parent.lexically_relative(root).string();
  if(file.filename()!="sources.xml" || relative==".." || relative.rfind("../",0)==0)throw std::runtime_error("Unmanaged source configuration");
  return parent/file.filename();
}
std::string Read(int directory,const char* name)
{
  Descriptor input(openat(directory,name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));struct stat info{};
  if(fstat(input.value,&info)!=0 || !S_ISREG(info.st_mode) || info.st_uid!=getuid() || info.st_size<0 || info.st_size>4194304)throw std::runtime_error("Unsafe source configuration");
  std::string bytes(static_cast<size_t>(info.st_size),'\0');size_t offset=0;
  while(offset<bytes.size()){const auto n=read(input.value,bytes.data()+offset,bytes.size()-offset);if(n<=0)throw std::runtime_error("Source configuration read failed");offset+=n;}
  return bytes;
}
void Atomic(int directory,const char* name,const std::string& bytes)
{
  unlinkat(directory,".isolde-source-write.tmp",0);
  Descriptor output(openat(directory,".isolde-source-write.tmp",O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));size_t offset=0;
  while(offset<bytes.size()){const auto n=write(output.value,bytes.data()+offset,bytes.size()-offset);if(n<=0)throw std::runtime_error("Source configuration write failed");offset+=n;}
  if(fsync(output.value)!=0 || renameat(directory,".isolde-source-write.tmp",directory,name)!=0 || fsync(directory)!=0)throw std::runtime_error("Source configuration publication failed");
}
void SaveAtomic()
{
  const auto file=SourceFile();Descriptor directory(open(file.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
  const auto temporary=file.parent_path()/".isolde-sources.xml";
  struct stat info{};if(fstatat(directory.value,temporary.filename().c_str(),&info,AT_SYMLINK_NOFOLLOW)==0)throw std::runtime_error("Unfinished source configuration publication");
  if(!CMediaSourceSettings::GetInstance().Save(temporary.string()))throw std::runtime_error("Kodi could not serialize sources");
  try { const auto bytes=Read(directory.value,temporary.filename().c_str());Atomic(directory.value,"sources.xml",bytes);unlinkat(directory.value,temporary.filename().c_str(),0); }
  catch(...){unlinkat(directory.value,temporary.filename().c_str(),0);throw;}
}
void BackupContent(int directory,CVideoDatabase& database,const std::string& path,const std::string& category)
{
  if(category=="music"){CVariant backup(CVariant::VariantTypeObject);backup["version"]=1;backup["category"]="music";std::string bytes;CJSONVariantWriter::Write(backup,bytes,true);Atomic(directory,"source-native-backup.json",bytes);return;}
  VIDEO::SScanSettings settings;bool direct=false;const auto scraper=database.GetScraperForPath(path,settings,direct);
  CVariant backup(CVariant::VariantTypeObject);backup["version"]=1;backup["path"]=path;
  backup["category"]="video";
  backup["content"]=direct && scraper?ADDON::TranslateContent(scraper->Content()):"";
  backup["scraper"]=direct && scraper?scraper->ID():"";
  backup["settings"]=direct && scraper?scraper->GetPathSettings():"";
  backup["recurse"]=direct?settings.recurse:1;backup["parentName"]=direct && settings.parent_name;
  backup["parentRoot"]=direct && settings.parent_name_root;backup["noUpdate"]=direct && settings.noupdate;
  backup["exclude"]=direct && settings.exclude;backup["allAudio"]=direct && settings.m_allExtAudio;
  std::string bytes;if(!CJSONVariantWriter::Write(backup,bytes,true) || bytes.size()>65536)throw std::runtime_error("Source recovery metadata exceeds limit");
  Atomic(directory,"source-native-backup.json",bytes);
}
void RestoreContent(int directory,CVideoDatabase& database)
{
  CVariant backup;if(!CJSONVariantParser::Parse(Read(directory,"source-native-backup.json"),backup) || backup["version"].asInteger()!=1)throw std::runtime_error("Invalid source recovery metadata");
  if(backup["category"].asString()=="music")return;
  if(backup["category"].asString()!="video")throw std::runtime_error("Unknown source recovery category");
  const auto path=backup["path"].asString(),content=backup["content"].asString(),id=backup["scraper"].asString();
  if(path.empty() || path.front()!='/' || path.size()>4096 || backup["settings"].asString().size()>65536)throw std::runtime_error("Invalid source recovery path");
  ADDON::ScraperPtr scraper;
  if(!content.empty())
  {
    ADDON::AddonPtr addon;
    if(!CServiceBroker::GetAddonMgr().GetAddon(id,addon,ADDON::OnlyEnabled::CHOICE_YES) || !(scraper=std::dynamic_pointer_cast<ADDON::CScraper>(addon)) || !scraper->SetPathSettings(ADDON::TranslateContent(content),backup["settings"].asString()))throw std::runtime_error("Original scraper unavailable for recovery");
  }
  VIDEO::SScanSettings settings;settings.recurse=static_cast<int>(backup["recurse"].asInteger());settings.parent_name=backup["parentName"].asBoolean();settings.parent_name_root=backup["parentRoot"].asBoolean();settings.noupdate=backup["noUpdate"].asBoolean();settings.exclude=backup["exclude"].asBoolean();settings.m_allExtAudio=backup["allAudio"].asBoolean();
  database.BeginTransaction();
  try{database.SetScraperForPath(path,scraper,settings);if(!database.CommitTransaction())throw std::runtime_error("Content recovery failed");}
  catch(...){database.RollbackTransaction();throw;}
}
void Publish(int directory, const CVariant& reply)
{
  std::string bytes;
  if(!CJSONVariantWriter::Write(reply,bytes,true) || bytes.size()>1048576)throw std::runtime_error("Source response exceeds limit");
  unlinkat(directory,"source-reply.tmp",0);
  Descriptor output(openat(directory,"source-reply.tmp",O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));
  size_t done=0;
  while(done<bytes.size()) { const auto n=write(output.value,bytes.data()+done,bytes.size()-done);if(n<=0)throw std::runtime_error("Source response write failed");done+=n; }
  if(fsync(output.value)!=0 || renameat(directory,"source-reply.tmp",directory,"source-reply.json")!=0 || fsync(directory)!=0)throw std::runtime_error("Source response publication failed");
}
CVariant Inventory(CVideoDatabase& database)
{
  CVariant rows(CVariant::VariantTypeArray);
  for(const auto& category:std::vector<std::string>{"video","music"})
  {
  auto* sources=CMediaSourceSettings::GetInstance().GetSources(category);
  if(!sources || sources->size()+rows.size()>128)throw std::runtime_error("Source limit exceeded");
  for(const auto& source:*sources)
  {
    VIDEO::SScanSettings settings;bool direct=false;
    const auto scraper=category=="video"?database.GetScraperForPath(source.strPath,settings,direct):ADDON::ScraperPtr{};
    CVariant row(CVariant::VariantTypeObject);
    row["name"]=source.strName;row["path"]=source.strPath;row["category"]=category;
    row["content"]=category=="music"?"music":direct && scraper ? ADDON::TranslateContent(scraper->Content()) : "";
    row["scraper"]=direct && scraper ? scraper->ID() : "";
    row["recursive"]=category=="music" || settings.recurse>0;row["folderNames"]=category=="video" && settings.parent_name;
    row["noUpdate"]=category=="video" && settings.noupdate;row["exclude"]=category=="video" && settings.exclude;
    row["singlePath"]=source.vecPaths.size()==1;
    rows.push_back(row);
  }
  }
  return rows;
}
void Change(CVideoDatabase& database,const CVariant& source)
{
  if(!source.isObject() || source.size()!=10 || !source["name"].isString() || !source["path"].isString() || !source["category"].isString()
     || !source["content"].isString() || !source["scraper"].isString() || !source["recursive"].isBoolean()
     || !source["folderNames"].isBoolean() || !source["noUpdate"].isBoolean() || !source["remove"].isBoolean() || !source["exclude"].isBoolean())
    throw std::runtime_error("Invalid source document");
  const auto name=source["name"].asString(),path=source["path"].asString(),content=source["content"].asString(),scraperId=source["scraper"].asString();
  const auto category=source["category"].asString();
  if(category!="video" && category!="music")throw std::runtime_error("Unsupported source category");
  if(name.empty() || name.size()>160 || path.empty() || path.size()>4096 || path.front()!='/' || path=="/"
     || path.find("/../")!=std::string::npos || path.find("/./")!=std::string::npos || path.find('\0')!=std::string::npos
     || name.find_first_of("\r\n")!=std::string::npos || scraperId.size()>160)
    throw std::runtime_error("Unsafe source name or path");
  if((category=="video" && content!="" && content!="movies" && content!="tvshows" && content!="musicvideos") || (category=="music" && (content!="music" || !scraperId.empty())))throw std::runtime_error("Unsupported source content");
  auto& manager=CMediaSourceSettings::GetInstance();auto* sources=manager.GetSources(category);
  if(!sources || sources->size()>128)throw std::runtime_error("Video source limit exceeded");
  const auto previous=*sources;
  auto found=std::find_if(sources->begin(),sources->end(),[&](const CMediaSource& item){return item.strPath==path;});
  const bool remove=source["remove"].asBoolean();
  if(found!=sources->end() && found->vecPaths.size()!=1)throw std::runtime_error("Multipath sources must remain unchanged");
  if(!remove && found==sources->end() && sources->size()>=128)throw std::runtime_error("Video source limit reached");
  ADDON::ScraperPtr scraper;
  VIDEO::SScanSettings previousSettings;bool direct=false;
  const auto previousScraper=category=="video"?database.GetScraperForPath(path,previousSettings,direct):ADDON::ScraperPtr{};
  if(category=="video" && !remove && !content.empty())
  {
    ADDON::AddonPtr addon;
    if(!CServiceBroker::GetAddonMgr().GetAddon(scraperId,addon,ADDON::OnlyEnabled::CHOICE_YES)
       || !(scraper=std::dynamic_pointer_cast<ADDON::CScraper>(addon))
       || !scraper->SetPathSettings(ADDON::TranslateContent(content),previousScraper && direct && previousScraper->ID()==scraperId && ADDON::TranslateContent(previousScraper->Content())==content?previousScraper->GetPathSettings():""))
      throw std::runtime_error("The selected Kodi scraper is unavailable");
  }
  VIDEO::SScanSettings settings=direct?previousSettings:VIDEO::SScanSettings{};
  if(!direct || (settings.recurse>0)!=source["recursive"].asBoolean())settings.recurse=source["recursive"].asBoolean()?2147483647:0;
  settings.parent_name=source["folderNames"].asBoolean();settings.noupdate=source["noUpdate"].asBoolean();settings.exclude=remove || source["exclude"].asBoolean();
  database.BeginTransaction();
  try
  {
    if(category=="video")database.SetScraperForPath(path,scraper,settings);
    if(remove) { if(found!=sources->end())sources->erase(found); }
    else
    {
      CMediaSource updated;if(found!=sources->end())updated=*found;
      if(found==sources->end())updated.FromNameAndPaths(category,name,{path});else updated.strName=name;
      if(found==sources->end())sources->push_back(updated);else *found=updated;
    }
    SaveAtomic();if(!database.CommitTransaction())throw std::runtime_error("Kodi could not save the source");
  }
  catch(...)
  {
    database.RollbackTransaction();*sources=previous;throw;
  }
}
}

int IsoldeSources(const std::vector<std::string>& parameters)
{
  const auto* managed=std::getenv("ISOLDE_MANAGED_KODI");
  if(!managed || std::string(managed)!="1" || parameters.size()!=1 || !Token(parameters[0]))return -1;
  try
  {
    Descriptor directory(open(CSpecialProtocol::TranslatePath("special://profile/isolde").c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
    Descriptor input(openat(directory.value,"source-request.json",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));
    struct stat info{};
    if(fstat(input.value,&info)!=0 || !S_ISREG(info.st_mode) || info.st_uid!=getuid() || info.st_size<=0 || info.st_size>1048576)return -1;
    std::string bytes(static_cast<size_t>(info.st_size),'\0');size_t offset=0;
    while(offset<bytes.size()){const auto n=read(input.value,bytes.data()+offset,bytes.size()-offset);if(n<=0)return -1;offset+=n;}
    CVariant request;
    if(!CJSONVariantParser::Parse(bytes,request) || request["version"].asInteger()!=1 || request["token"].asString()!=parameters[0])return -1;
    CVariant reply(CVariant::VariantTypeObject);reply["version"]=1;reply["token"]=parameters[0];reply["ok"]=false;
    try
    {
      CVideoDatabase database;
      if(!database.Open())throw std::runtime_error("Kodi video library unavailable");
      const auto operation=request["operation"].asString();
      if(operation=="set" || operation=="restore")
      {
        if(CVideoLibraryQueue::GetInstance().IsScanningLibrary() || CMusicLibraryQueue::GetInstance().IsScanningLibrary())throw std::runtime_error("Wait for the Kodi scan to finish");
        const auto file=SourceFile();Descriptor config(open(file.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
        if(operation=="set")
        {
          if(request["expected"]!=Inventory(database))throw std::runtime_error("Sources changed after preview");
          struct stat backup{};if(fstatat(directory.value,"source-config-backup.xml",&backup,AT_SYMLINK_NOFOLLOW)==0)throw std::runtime_error("Previous source transaction needs recovery");
          struct stat existing{};
          const auto bytes=fstatat(config.value,"sources.xml",&existing,AT_SYMLINK_NOFOLLOW)==0?Read(config.value,"sources.xml"):"<sources/>";
          Atomic(directory.value,"source-config-backup.xml",bytes);
          BackupContent(directory.value,database,request["source"]["path"].asString(),request["source"]["category"].asString());
        }
        else
        {
          struct stat existing{};
          if(fstatat(directory.value,"source-config-backup.xml",&existing,AT_SYMLINK_NOFOLLOW)==0)
          {
            Read(directory.value,"source-config-backup.xml");
            if(!CMediaSourceSettings::GetInstance().Load(CSpecialProtocol::TranslatePath("special://profile/isolde/source-config-backup.xml")))throw std::runtime_error("Source backup cannot be loaded");
          }
          unlinkat(config.value,".isolde-sources.xml",0);
        }
        if(operation=="set")Change(database,request["source"]);
        else
        {
          struct stat metadata{};
          if(fstatat(directory.value,"source-native-backup.json",&metadata,AT_SYMLINK_NOFOLLOW)==0)RestoreContent(directory.value,database);
        }
        if(operation=="restore")
        {
          struct stat backup{};
          if(fstatat(directory.value,"source-config-backup.xml",&backup,AT_SYMLINK_NOFOLLOW)==0)Atomic(config.value,"sources.xml",Read(directory.value,"source-config-backup.xml"));
          if(request["hadConfig"].isBoolean() && !request["hadConfig"].asBoolean()){if(unlinkat(config.value,"sources.xml",0)!=0 && errno!=ENOENT)throw std::runtime_error("Source rollback removal failed");fsync(config.value);}
        }
      }
      else if(operation!="get")throw std::runtime_error("Unsupported source operation");
      reply["sources"]=Inventory(database);reply["sourceFileExists"]=std::filesystem::exists(SourceFile());reply["ok"]=true;
    }
    catch(const std::exception&){reply["error"]="Kodi could not apply the requested source operation. Refresh or recover before retrying.";}
    Publish(directory.value,reply);return reply["ok"].asBoolean()?0:-1;
  }
  catch(...){return -1;}
}
