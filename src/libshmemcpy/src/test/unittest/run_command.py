from typing import List
import os
import subprocess as sp


Fail_MSG = 'Failed to run command: '


def run_local_command(cmd: List[str], env: dict = None, capture = True, allow_fail=False):
    if env:
        new_env = dict(os.environ, **env)
    else:
        new_env = os.environ
    print(f"{' '.join(cmd)}")
    if capture:
        ret = sp.run(cmd, stdout=sp.PIPE, stderr=sp.PIPE,
                     encoding='utf8', env=new_env)
    else:
        ret = sp.run(cmd, encoding='utf8', stderr=sys.stderr, stdout=sys.stdout,
                     env=new_env)
    if not allow_fail and ret.returncode != 0:
        if capture:
            raise RuntimeError(Fail_MSG + ' '.join(cmd) + '\n' + ret.stderr)
        else:
            raise RuntimeError(Fail_MSG + ' '.join(cmd) + '\n')
    return ret
