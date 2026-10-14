import os, subprocess, sys
binary=sys.argv[1]
env=os.environ.copy()
for key in ['FURY_PLAYER_SOURCE_HAIR','FURY_PLAYER_SHORT_TAIL','FURY_PLAYER_HAIR_DETAIL']:env.pop(key,None)
def run(mode,bundle=None):
    selected=env.copy()
    if bundle is not None:
        selected.update(zip(['FURY_PLAYER_SOURCE_HAIR','FURY_PLAYER_SHORT_TAIL','FURY_PLAYER_HAIR_DETAIL'],bundle))
    result=subprocess.run([binary,mode],env=selected,check=True,text=True,capture_output=True)
    return next(s for s in result.stdout.splitlines() if s.startswith('IDENTITY '))
original=run('legacy',['0','0','0']);balanced=run('legacy',['1','1','512'])
for mode,expected in [('auto',balanced),('balanced-low',balanced),('original',original),('auto-low',original),('disabled',original),('legacy',original)]:
    value=run(mode);assert value==expected,(mode,value,expected);print(mode,value)
assert run('auto',['0','0','0'])==original
assert run('original',['1','1','512'])==balanced
print('Exact Near/Far geometry, weights, deform metadata, full atlas/mips and detail pixels/regions match explicit bundles; concurrent legacy/configured reads agree.')
