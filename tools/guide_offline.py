"""The same denied guest namespace used by preflight and real off-time playback."""
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile

PROBE = '''import json,os,socket,sys
from pathlib import Path
root=Path(sys.argv[1]);errors=[]
for address in [('1.1.1.1',443),('8.8.8.8',53)]:
    with socket.socket() as connection:
        connection.settimeout(0.5);code=connection.connect_ex(address);errors.append(code)
        if code==0:raise RuntimeError('External network unexpectedly available')
proof={'namespace':os.readlink('/proc/self/ns/net'),'externalConnectErrors':errors,'interfaces':list(socket.if_nameindex()),'routes':Path('/proc/net/route').read_text(),'parentNamespace':sys.argv[2],'pid':os.getpid()}
if proof['namespace']==proof['parentNamespace'] or any(name!='lo' for _,name in proof['interfaces']):raise RuntimeError('Network namespace was not denied')
(root/'network-denial.json').write_text(json.dumps(proof,indent=2))
if len(sys.argv)>3:os.execv(sys.argv[3],[sys.argv[3],'--windowed'])
'''


def command(arguments):
    sandbox=Path('/usr/lib/isolde-native/bwrap')
    for path in (sandbox,*sandbox.parents):
        info=path.lstat()
        if path.is_symlink() or info.st_uid!=0 or info.st_mode & (stat.S_IWGRP|stat.S_IWOTH):raise RuntimeError('Untrusted native network sandbox path')
    if not sandbox.is_file() or not os.access(sandbox,os.X_OK):raise RuntimeError('Verified native network sandbox unavailable')
    return [str(sandbox),'--unshare-user','--unshare-net','--die-with-parent','--cap-drop','ALL','--bind','/','/','--proc','/proc','--dev-bind','/dev','/dev','--',*arguments]


def preflight():
    with tempfile.TemporaryDirectory(prefix='isolde-guide-network-') as folder:
        root=Path(folder);script=root/'denied.py';script.write_text(PROBE)
        result=subprocess.run(command(['/usr/bin/python3',str(script),str(root),os.readlink('/proc/self/ns/net')]),text=True,capture_output=True,timeout=10)
        if result.returncode:raise RuntimeError('Offline namespace preflight failed: '+result.stderr[-2048:])
        proof=json.loads((root/'network-denial.json').read_text())
        assert proof['namespace']!=proof['parentNamespace'] and len(proof['externalConnectErrors'])==2 and all(type(value) is int and value!=0 for value in proof['externalConnectErrors'])
        print('T11 actual denied-network namespace preflight: '+json.dumps(proof),flush=True)


if __name__=='__main__':
    if sys.argv[1:]!=['--self-test']:raise SystemExit('Use the bounded VM runner')
    preflight()
