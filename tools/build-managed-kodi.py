#!/usr/bin/env python3
"""Build the existing Kodi dependency with one managed-only source builtin.

Guest-only, called by vm-test Configure. Source authenticity is supplied by the
installed distribution's signed APT source index. No host tools or policy changes.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import shlex
import socket
import subprocess
import time

def run(arguments, cwd=None, timeout=300):
    print('KODI_BUILD_STAGE '+arguments[0]+' '+('configure' if '-S' in arguments else 'build' if '--build' in arguments else arguments[1] if len(arguments)>1 else ''),flush=True)
    return subprocess.run(arguments,cwd=cwd,check=True,text=True,timeout=timeout)

def sha(path):
    digest=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):digest.update(block)
    return digest.hexdigest()

def atomic(path,value):
    temporary=path.with_suffix('.tmp')
    with temporary.open('w') as stream:json.dump(value,stream,indent=2);stream.flush();os.fsync(stream.fileno())
    os.replace(temporary,path)

def normalize_package(package):
    # dpkg preserves modes on pre-existing directories. Repair only this
    # package's fixed, root-owned, non-linked directories during configuration.
    (package/'DEBIAN/postinst').write_text('#!/bin/sh\nset -eu\nfor directory in /opt/isolde-managed-kodi /opt/isolde-managed-kodi/bin /opt/isolde-managed-kodi/licenses; do\n  [ ! -L "$directory" ] && [ -d "$directory" ] && [ "$(/usr/bin/stat -c %u -- "$directory")" = 0 ] || exit 1\n  /bin/chmod 0755 -- "$directory"\ndone\n')
    for path in [package,*package.rglob('*')]:
        if path.resolve()!=path or path.is_symlink() or path.stat().st_uid!=os.getuid():raise RuntimeError('Unsafe managed Kodi package staging path')
        if path.is_dir():path.chmod(0o755)
        elif path.is_file():path.chmod(0o755 if path.name=='kodi.bin' or path in (package/'opt/isolde-managed-kodi/bin/kodi',package/'DEBIAN/postinst') else 0o644)
        else:raise RuntimeError('Unexpected managed Kodi package entry')

def repair_package_modes(work,source,receipt):
    package=work/'package';prefix=package/'opt/isolde-managed-kodi'
    metadata=json.loads((prefix/'manifest.json').read_text())
    expected=hashlib.sha256((source/'native/kodi/IsoldeSources.cpp').read_bytes()+(source/'native/kodi/IsoldeSources.h').read_bytes()).hexdigest()
    binary=prefix/'kodi.bin';installed=Path('/opt/isolde-managed-kodi/kodi.bin')
    if metadata.get('feature')!='isolde-sources-v1' or metadata.get('extensionSha256')!=expected or metadata.get('binarySha256')!=sha(binary) or sha(installed)!=sha(binary):raise RuntimeError('Package-mode repair requires the exact built and installed native candidate')
    normalize_package(package)
    control=(package/'DEBIAN/control').read_text()
    version=re.search(r'^Version: ([0-9A-Za-z.+~:-]+)$',control,re.M)
    architecture=re.search(r'^Architecture: ([a-z0-9]+)$',control,re.M)
    if not version or not architecture:raise RuntimeError('Invalid managed Kodi package identity')
    deb=work/('isolde-managed-kodi_'+version[1]+'_'+architecture[1]+'.deb')
    run(['dpkg-deb','--root-owner-group','--build',str(package),str(deb)],timeout=120)
    run(['sudo','-n','apt-get','install','--reinstall','-y',str(deb)],timeout=300)
    for path in (installed.parent,installed.parent/'bin',installed,installed.parent/'bin/kodi',installed.parent/'manifest.json'):
        info=path.stat()
        if path.resolve()!=path or info.st_uid!=0 or info.st_mode&0o022:raise RuntimeError('Managed Kodi package permissions still unsafe: '+str(path)+' uid='+str(info.st_uid)+' mode='+oct(info.st_mode&0o7777)+' resolved='+str(path.resolve()))
    atomic(receipt,dict(passed=True,kind='dependency-package-permission-repair',binarySha256=sha(installed),package=str(deb),packageSha256=sha(deb)))

def overlay(source,repository,version):
    native=repository/'native/kodi';target=source/'xbmc/interfaces/builtins'
    for name in ('IsoldeSources.cpp','IsoldeSources.h'):shutil.copyfile(native/name,target/name)
    edits=[(target/'CMakeLists.txt','            LibraryBuiltins.cpp','            IsoldeSources.cpp\n            LibraryBuiltins.cpp'),
           (target/'LibraryBuiltins.cpp','#include "LibraryBuiltins.h"','#include "LibraryBuiltins.h"\n#include "IsoldeSources.h"'),
           (target/'LibraryBuiltins.cpp','  return {\n','  return {\n          {"isoldesources", {"Managed Isolde source transaction", 1, IsoldeSources}},\n')]
    for path,before,after in edits:
        text=path.read_text()
        if after in text:continue
        if text.count(before)!=1:raise RuntimeError('Kodi source builtin layout changed; inspect before building')
        path.write_text(text.replace(before,after))
    # Bounded managed-backend diagnostics for native playback publication.
    # They observe Kodi's real message path without changing its ownership.
    traces=[('xbmc/guilib/GUIWindowManager.cpp','  bool handled = false;',
             '  bool handled = false;\n  if (std::getenv("ISOLDE_MANAGED_KODI") && message.GetMessage()==GUI_MSG_PLAYBACK_STARTED) CLog::Log(LOGINFO,"ISOLDE native playback dispatch targets={}",m_vecMsgTargets.size());'),
            ('xbmc/guilib/GUIWindowManager.cpp','  m_vecMsgTargets.emplace_back(pMsgTarget);',
             '  m_vecMsgTargets.emplace_back(pMsgTarget);\n  if (std::getenv("ISOLDE_MANAGED_KODI")) CLog::Log(LOGINFO,"ISOLDE native target added count={} manager={}",m_vecMsgTargets.size(),static_cast<void*>(this));'),
            ('xbmc/guilib/GUIWindowManager.cpp','  m_vecMsgTargets.erase( m_vecMsgTargets.begin(), m_vecMsgTargets.end() );',
             '  if (std::getenv("ISOLDE_MANAGED_KODI")) CLog::Log(LOGINFO,"ISOLDE native targets cleared count={} manager={}",m_vecMsgTargets.size(),static_cast<void*>(this));\n  m_vecMsgTargets.erase( m_vecMsgTargets.begin(), m_vecMsgTargets.end() );'),
            ('xbmc/application/Application.cpp','  case GUI_MSG_PLAYBACK_STARTED:\n    {',
             '  case GUI_MSG_PLAYBACK_STARTED:\n    {\n      if (std::getenv("ISOLDE_MANAGED_KODI")) CLog::Log(LOGINFO,"ISOLDE application received playback start");'),
            ('xbmc/application/Application.cpp','          std::make_shared<CFileItem>(*std::static_pointer_cast<CFileItem>(message.GetItem()));\n      m_playerEvent.Reset();',
             '          std::make_shared<CFileItem>(*std::static_pointer_cast<CFileItem>(message.GetItem()));\n      if (std::getenv("ISOLDE_MANAGED_KODI")) CLog::Log(LOGINFO,"ISOLDE application current pathBytes={} tagged={} id={}",m_itemCurrentFile->GetPath().size(),m_itemCurrentFile->HasVideoInfoTag(),m_itemCurrentFile->HasVideoInfoTag()?m_itemCurrentFile->GetVideoInfoTag()->m_iDbId:-1);\n      m_playerEvent.Reset();'),
            ('xbmc/application/ApplicationPlayerCallback.cpp','  std::shared_ptr<CFileItem> itemCurrentFile;',
             '  std::shared_ptr<CFileItem> itemCurrentFile;\n  if (std::getenv("ISOLDE_MANAGED_KODI")) CLog::Log(LOGINFO,"ISOLDE native callback pathBytes={} tagged={} id={}",file.GetPath().size(),file.HasVideoInfoTag(),file.HasVideoInfoTag()?file.GetVideoInfoTag()->m_iDbId:-1);'),
            ('xbmc/application/ApplicationSkinHandling.cpp','  CServiceBroker::GetGUI()->GetWindowManager().AddMsgTarget(m_msgCb);',
             '  if (std::getenv("ISOLDE_MANAGED_KODI")) CLog::Log(LOGINFO,"ISOLDE native skin application target={}",m_msgCb!=nullptr);\n  CServiceBroker::GetGUI()->GetWindowManager().AddMsgTarget(m_msgCb);')]
    for name,before,after in traces:
        path=source/name;text=path.read_text()
        if after not in text:
            if text.count(before)!=1:raise RuntimeError('Native playback diagnostic source layout changed: '+name)
            text=text.replace(before,after)
        if '#include <cstdlib>' not in text:text='#include <cstdlib>\n'+text
        if '#include "GUIUserMessages.h"' not in text:text='#include "GUIUserMessages.h"\n'+text
        if text!=path.read_text():path.write_text(text)
    path=source/'xbmc/application/ApplicationSkinHandling.cpp';text=path.read_text()
    before='void CApplicationSkinHandling::UnloadSkin()\n{'
    after=before+'\n  if (std::getenv("ISOLDE_MANAGED_KODI"))\n  {\n    void* frames[12];\n    const int count=backtrace(frames,12);\n    char** symbols=backtrace_symbols(frames,count);\n    if (symbols) { for (int i=0;i<count;++i) CLog::Log(LOGINFO,"ISOLDE native unload caller {}",symbols[i]); std::free(symbols); }\n  }'
    if after not in text:
        if text.count(before)!=1:raise RuntimeError('Native skin unload source layout changed')
        text=text.replace(before,after)
    if '#include <execinfo.h>' not in text:text='#include <execinfo.h>\n'+text
    if text!=path.read_text():path.write_text(text)
    # X11's forced context refresh unloads the skin even when EGL reuses its
    # context. Tell the existing resize/fullscreen caller to reload that skin;
    # otherwise its application/playlist message targets stay disconnected.
    path=source/'xbmc/windowing/X11/WinSystemX11GLContext.cpp'
    text=path.read_text()
    before='    if (force)\n      CRenderSystemGL::InitRenderSystem();'
    after='    if (force)\n    {\n      CRenderSystemGL::InitRenderSystem();\n      if (std::getenv("ISOLDE_MANAGED_KODI")) m_newGlContext = true;\n    }'
    if after not in text:
        if text.count(before)!=1:raise RuntimeError('X11 context refresh source layout changed')
        text=text.replace(before,after)
    if '#include <cstdlib>' not in text:text='#include <cstdlib>\n'+text
    if text!=path.read_text():path.write_text(text)
    header=source/'xbmc/application/ApplicationSkinHandling.h';text=header.read_text()
    member='  mutable bool m_isoldePendingSkinReload{false};\n'
    if member not in text:
        text,count=re.subn(r'(?m)^(\s*bool\s*&\s*m_bInitializing;)',lambda match:member+match[1],text)
        if count!=1:raise RuntimeError('Native skin initialization member layout changed')
        header.write_text(text)
    path=source/'xbmc/application/ApplicationSkinHandling.cpp';text=path.read_text()
    edits=[('void CApplicationSkinHandling::ReloadSkin(bool confirm)\n{',
            'void CApplicationSkinHandling::ReloadSkin(bool confirm)\n{\n  if (m_bInitializing && std::getenv("ISOLDE_MANAGED_KODI") && g_SkinInfo && !CServiceBroker::GetGUI()->GetWindowManager().Initialized())\n    m_isoldePendingSkinReload = true;'),
           ('void CApplicationSkinHandling::ProcessSkin() const\n{',
            'void CApplicationSkinHandling::ProcessSkin() const\n{\n  if (m_isoldePendingSkinReload && !m_bInitializing)\n  {\n    m_isoldePendingSkinReload = false;\n    CServiceBroker::GetAppMessenger()->PostMsg(TMSG_EXECUTE_BUILT_IN, -1, -1, nullptr, "ReloadSkin");\n  }')]
    edits.append(('      default:\n        break;',
                  '      default:\n        if (std::getenv("ISOLDE_MANAGED_KODI"))\n          CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(WINDOW_FULLSCREEN_VIDEO);\n        break;'))
    for before,after in edits:
        if after not in text:
            if text.count(before)!=1:raise RuntimeError('Native deferred skin reload source layout changed')
            text=text.replace(before,after)
    if text!=path.read_text():path.write_text(text)
    # The distribution patch reads DEB_VERSION only in this translation unit.
    # Retain that version stamp without invalidating every cached Kodi object.
    stamp=source/'xbmc/utils/IsoldeDistributionVersion.h'
    stamp.write_text('#pragma once\n#ifndef DEB_VERSION\n#define DEB_VERSION '+json.dumps(version)+'\n#endif\n')
    info=source/'xbmc/utils/SystemInfo.cpp';text=info.read_text()
    anchor='#include "SystemInfo.h"';replacement=anchor+'\n#include "IsoldeDistributionVersion.h"'
    if replacement not in text:
        if text.count(anchor)!=1:raise RuntimeError('Distribution version source layout changed')
        info.write_text(text.replace(anchor,replacement))

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,required=True);parser.add_argument('--work',type=Path,required=True);parser.add_argument('--receipt',type=Path,required=True);parser.add_argument('--repair-package-modes',action='store_true')
    args=parser.parse_args()
    if socket.gethostname()!='isolde-test' or os.geteuid()==0:raise RuntimeError('Build only as the normal Isolde-Test guest user')
    expected=Path.home()/'isolde-oracle/support/managed-kodi'
    if args.work.absolute()!=expected or args.work.resolve()!=expected:raise RuntimeError('Unsafe Kodi build workspace')
    if args.source.resolve()!=args.source.absolute() or not (args.source/'native/kodi/upstream.json').is_file():raise RuntimeError('Verified Isolde source required')
    work=args.work;work.mkdir(parents=True,exist_ok=True,mode=0o700)
    if args.repair_package_modes:
        repair_package_modes(work,args.source,args.receipt);return
    if shutil.disk_usage(work).free<2*1024**3:raise RuntimeError('Managed Kodi build needs at least 2 GiB free before setup')
    pin=json.loads((args.source/'native/kodi/upstream.json').read_text())
    version=subprocess.check_output(['dpkg-query','-W','-f=${Version}','kodi-bin'],text=True).strip()
    print('INSTALLED_KODI_VERSION '+version,flush=True)
    if not re.fullmatch(r'(?:\d+:)?21\.[0-9]+[^\s/]*',version):raise RuntimeError('Managed source extension supports the Kodi 21 Omega API family; observed '+version)
    source_config=work/'apt-sources.sources'
    entries=[]
    for file in sorted(Path('/etc/apt/sources.list.d').glob('*.sources')):
        text=file.read_text()
        for block in text.split('\n\n'):
            trusted_distribution=any(key in block for key in ('debian-archive-keyring.gpg','ubuntu-archive-keyring.gpg'))
            if re.search(r'^Types:\s+deb(?:\s|$)',block,re.M) and trusted_distribution and not re.search(r'^Enabled:\s+no',block,re.M):
                entries.append(re.sub(r'^Types:.*$', 'Types: deb deb-src',block,flags=re.M))
    if not entries:raise RuntimeError('No existing signed distribution source configuration; no new repository will be added')
    source_config.write_text('\n\n'.join(entries)+'\n')
    apt=['-o','Dir::Etc::sourcelist='+str(source_config),'-o','Dir::Etc::sourceparts=-','-o','APT::Get::List-Cleanup=0']
    run(['sudo','-n','apt-get',*apt,'update'],timeout=300)
    run(['sudo','-n','apt-get',*apt,'build-dep','-y','kodi='+version],timeout=900)
    downloads=work/'distribution-source';downloads.mkdir(exist_ok=True)
    # APT checks the dsc/source hashes against the authenticated source index.
    if not list(downloads.glob('kodi-*/CMakeLists.txt')):
        run(['apt-get',*apt,'source','kodi='+version],cwd=downloads,timeout=600)
    candidates=list(downloads.glob('kodi-*/CMakeLists.txt'))
    if len(candidates)!=1:raise RuntimeError('Ambiguous distribution source checkout')
    source=candidates[0].parent
    identity=work/'source-identity.json'
    documents=sorted(downloads.glob('*.dsc'))
    if len(documents)!=1:raise RuntimeError('One authenticated source descriptor is required')
    record=dict(version=version,dscSha256=sha(documents[0]),feature=pin['feature'])
    if identity.exists() and json.loads(identity.read_text())!=record:raise RuntimeError('Kodi source changed; preserve the old build and inspect')
    atomic(identity,record)
    overlay(source,args.source,version)
    build=work/'build'
    rules=(source/'debian/rules').read_text();shutil.copyfile(source/'debian/rules',args.receipt.parent/'kodi-distribution-rules.txt')
    # Honor all distribution feature/dependency switches together. DFSG source
    # deliberately omits upstream vendored dependency/version files.
    distribution_flags=re.findall(r'^(-DENABLE_[A-Za-z0-9_]+=(?:ON|OFF))\s*\\?\s*$',rules,re.M)
    dvd_flags=[]
    for name,component in (('DVDREAD','libdvdread-embedded'),('DVDNAV','libdvdnav-embedded')):
        dependency=source/component
        if not dependency.is_dir() or dependency.is_symlink():raise RuntimeError('Authenticated embedded DVD component missing')
        archive=source/'tools/depends/target'/component.replace('-embedded','')/(component+'.tar.xz')
        if not archive.exists():run(['tar','-cf',str(archive),component],cwd=source,timeout=60)
        dvd_flags.extend(['-DLIB'+name+'_URL='+str(archive),'-DLIB'+name+'_HASH=SHA256='+sha(archive)])
    flag_environment=dict(os.environ,DEB_BUILD_MAINT_OPTIONS='hardening=+all,-bindnow')
    def flags(kind):
        values=shlex.split(subprocess.check_output(['dpkg-buildflags','--get',kind],cwd=source,env=flag_environment,text=True))
        return ' '.join(value for value in values if not value.startswith('-flto') and value!='-ffat-lto-objects')
    cpp=flags('CPPFLAGS')+' -D_XBMC'
    run(['cmake','-S',str(source),'-B',str(build),'-G','Ninja',*distribution_flags,*dvd_flags,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_C_FLAGS='+cpp+' '+flags('CFLAGS'),'-DCMAKE_CXX_FLAGS='+cpp+' '+flags('CXXFLAGS'),'-DCMAKE_EXE_LINKER_FLAGS='+flags('LDFLAGS'),'-DCMAKE_C_FLAGS_RELEASE=-O2 -DNDEBUG -g0','-DCMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG -g0','-DCMAKE_INSTALL_PREFIX=/usr','-DCORE_PLATFORM_NAME=x11','-DAPP_RENDER_SYSTEM=gl','-DUSE_LTO=OFF','-DENABLE_TESTING=OFF'],timeout=300)
    # Kodi's CMake target is kodi; this distribution names X11 output kodi-x11.
    run(['cmake','--build',str(build),'--target','kodi','--parallel','2'],timeout=5400)
    binary=build/'kodi-x11'
    if not binary.is_file():raise RuntimeError('Kodi build did not produce its Linux executable')
    with binary.open('rb') as stream:
        if stream.read(4)!=b'\x7fELF':raise RuntimeError('Kodi output is not a Linux executable')
    binary_hash=sha(binary)
    extension_hash=hashlib.sha256((args.source/'native/kodi/IsoldeSources.cpp').read_bytes()+(args.source/'native/kodi/IsoldeSources.h').read_bytes()).hexdigest()
    package=work/'package';prefix=package/'opt/isolde-managed-kodi';(prefix/'bin').mkdir(parents=True,exist_ok=True)
    shutil.copyfile(binary,prefix/'kodi.bin');(prefix/'kodi.bin').chmod(0o755)
    (prefix/'bin/kodi').write_text('#!/bin/sh\nexport KODI_HOME=/usr/share/kodi\nexec /opt/isolde-managed-kodi/kodi.bin "$@"\n');(prefix/'bin/kodi').chmod(0o755)
    metadata=dict(feature=pin['feature'],sourceApiBaseline=pin['release'],upstreamCommit=pin['commit'],distributionSource=record,binarySha256=binary_hash,extensionSha256=extension_hash)
    atomic(prefix/'manifest.json',metadata);(prefix/'manifest.json').chmod(0o644)
    control=package/'DEBIAN';control.mkdir(exist_ok=True)
    architecture=subprocess.check_output(['dpkg','--print-architecture'],text=True).strip()
    package_version=re.search(r'21\.[0-9]+',version)[0]+'+isolde.'+extension_hash[:12]
    (control/'control').write_text('Package: isolde-managed-kodi\nVersion: '+package_version+'\nArchitecture: '+architecture+'\nMaintainer: Isolde project\nDepends: kodi-bin (= '+version+')\nDescription: Kodi managed source interface for Isolde\n Built from authenticated distribution source with its compatibility patches.\n')
    licenses=prefix/'licenses';licenses.mkdir(exist_ok=True)
    for name in ('LICENSE.md','LICENSES/README.md'):
        if (source/name).is_file():shutil.copyfile(source/name,licenses/Path(name).name)
    deb=work/('isolde-managed-kodi_'+package_version+'_'+architecture+'.deb')
    normalize_package(package)
    run(['dpkg-deb','--root-owner-group','--build',str(package),str(deb)],timeout=120)
    # Content hashes identify exact candidates; their lexical Debian version
    # ordering does not indicate whether a candidate is newer.
    run(['sudo','-n','apt-get','install','-y','--allow-downgrades','--reinstall',str(deb)],timeout=300)
    installed=Path('/opt/isolde-managed-kodi/kodi.bin')
    if installed.is_symlink() or installed.stat().st_uid!=0 or sha(installed)!=binary_hash:raise RuntimeError('Installed managed Kodi binary differs from the built artifact')
    atomic(args.receipt,dict(passed=True,kind='dependency-build-not-application-acceptance',metadata=metadata,package=str(deb),packageSha256=sha(deb),normalProfilesUsed=False,serviceRestarted=False,createdAt=time.time()))
    print('MANAGED_KODI_READY '+str(args.receipt),flush=True)

if __name__=='__main__':main()
