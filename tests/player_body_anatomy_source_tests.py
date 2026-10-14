#!/usr/bin/env python3
"""Independent PlayerBody CC0 correspondence/topology audit, with corrupted-output controls.

Does not import either producer. Reads pinned OBJ/target bytes, independently
reconstructs the selected source surfaces, then inspects emitted C++ arrays.
"""
import collections
import gzip
import hashlib
import json
import math
from pathlib import Path
import re
import struct

ROOT=Path(__file__).resolve().parents[1]
ASSETS=ROOT/'third_party/player_body_anatomy'
def require(ok,message):
    if not ok: raise AssertionError(message)
def f32(x): return struct.unpack('<f',struct.pack('<f',x))[0]
def array(text,name,width=1):
    match=re.search(r'\b'+re.escape(name)+r'\s*=\s*\{\{(.*?)\}\};',text,re.S)
    require(match is not None,'Missing generated array '+name)
    values=[float(x.rstrip('fu')) for x in re.findall(r'[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?[fu]?',match[1])]
    if width==1:return [int(x) for x in values]
    require(len(values)%width==0,'Array shape '+name)
    return [tuple(f32(x) for x in values[i:i+width]) for i in range(0,len(values),width)]
def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def unit(a):
    n=math.sqrt(dot(a,a));require(n>1e-12,'Nondegenerate source frame');return tuple(x/n for x in a)
def mean(points):return tuple(sum(p[k] for p in points)/len(points) for k in range(3))
def rejects(action,label):
    try:action()
    except (AssertionError,KeyError,ValueError):return
    raise AssertionError('Negative control did not reject '+label)

manifest=json.loads((ASSETS/'MANIFEST.json').read_text())
require(manifest['asset_license']=='CC0-1.0' and manifest['commit']=='d0a32e57a7f915cb2f2b95410e2117648c7bbb7e','Pinned CC0 source identity')
for record in manifest['shared_files']:
    data=(ROOT/record['path']).read_bytes()
    require(hashlib.sha256(data).hexdigest()==record['sha256'],'Input byte hash '+record['path'])
    require(hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==record['git_blob_sha1'],'Input Git blob '+record['path'])
    require(len(data)==record['bytes'],'Input byte length')
positions=[];uv=[];faces=[];groups=[]
for line in (ROOT/'third_party/mpfb_assets/base.obj').read_text().splitlines():
    data=line.split()
    if not data:continue
    if data[0]=='v':positions.append(tuple(map(float,data[1:4])))
    elif data[0]=='vt':uv.append(tuple(map(float,data[1:3])))
    elif data[0]=='g':groups=data[1:]
    elif data[0]=='f':faces.append((tuple(groups),tuple(tuple(int(x)-1 for x in corner.split('/')[:2]) for corner in data[1:])))
recipe=json.loads((ASSETS/'player_body_recipe.json').read_text())
morphed=[list(p) for p in positions]
for target in recipe['targets']:
    for line in gzip.decompress((ROOT/target['path']).read_bytes()).decode().splitlines():
        fields=line.split('#',1)[0].split()
        if not fields:continue
        index=int(fields[0]);delta=list(map(float,fields[1:]));require(len(delta)==3,'Graphical target arity')
        for k in range(3):morphed[index][k]+=target['weight']*delta[k]
body=sorted({v for groups,corners in faces if 'body' in groups for v,_ in corners})
floor=min(morphed[i][1] for i in body);crown=max(morphed[i][1] for i in body);scale=.993/(crown-floor)
normalized=[(p[0]*scale,(p[1]-floor)*scale-.5,p[2]*scale) for p in morphed]
header=(ROOT/'engine/include/fury/generated/player_body_anatomy.hpp').read_text()
source_ids=array(header,'source_vertex_indices');source_uvs=array(header,'source_uv_indices')
actual_positions=array(header,'positions',3);actual_uv=array(header,'uv_coordinates',2)
actual_indices=array(header,'indices');actual_uv_indices=array(header,'uv_indices')
face_ids=array(header,'triangle_source_faces');global_ids=array(header,'triangle_source_indices')
selected=[];selected_faces=[];selected_global=[];offset=0
for face_id,(groups,corners) in enumerate(faces):
    first=offset;offset+=len(corners)-2
    if 'body' not in groups or any(normalized[v][1]<recipe['selection']['minimum_face_y'] for v,_ in corners):continue
    for k in range(1,len(corners)-1):
        selected.append((corners[0],corners[k],corners[k+1]));selected_faces.append(face_id);selected_global.append(first+k-1)
expected_ids=sorted({i for t in selected for i,_ in t});expected_uvs=sorted({u for t in selected for _,u in t})
vmap={v:i for i,v in enumerate(expected_ids)};umap={u:i for i,u in enumerate(expected_uvs)}
expected_positions=[tuple(f32(x) for x in normalized[i]) for i in expected_ids]
expected_indices=[vmap[i] for t in selected for i,_ in t]
expected_uv_indices=[umap[u] for t in selected for _,u in t]
require(source_ids==expected_ids and source_uvs==expected_uvs,'Complete independent source corner selection')
require(actual_positions==expected_positions,'Every normalized head position exactly matches independent reconstruction')
require(actual_indices==expected_indices,'Every source head triangle and winding retained')
require(actual_uv==[tuple(f32(x) for x in uv[i]) for i in expected_uvs] and actual_uv_indices==expected_uv_indices,'Per-corner UV seam correspondence')
require(face_ids==selected_faces and global_ids==selected_global,'Global source face/triangle correspondence')
corrupt=list(actual_positions);corrupt[0]=(corrupt[0][0]+.001,*corrupt[0][1:])
rejects(lambda:require(corrupt==expected_positions,'Corrupt coordinate'),'head coordinate')
corrupt=list(actual_indices);corrupt[1],corrupt[2]=corrupt[2],corrupt[1]
rejects(lambda:require(corrupt==expected_indices,'Corrupt winding'),'head winding')
corrupt=list(actual_uv_indices);corrupt[0]=(corrupt[0]+1)%len(actual_uv)
rejects(lambda:require(corrupt==expected_uv_indices,'Corrupt corner UV'),'head UV')

# Independently reconstruct the Head membership from immutable chart UVs.
# The external chart's lowest vertices include the jaw below generic blend Y.
head_members=set()
mouth_members=set()
for t in range(0,len(actual_indices),3):
    source_uv=actual_uv[actual_uv_indices[t]]
    neck_chart=.2151318<=source_uv[0]<=.5459292 and .8406228<=source_uv[1]<=.9738562
    mouth_chart=.7576338<=source_uv[0]<=.9237402 and .0118628<=source_uv[1]<=.1364402
    if not neck_chart: head_members.update(actual_indices[t:t+3])
    if mouth_chart: mouth_members.update(actual_indices[t:t+3])
rigid=array(header,'head_region')
require(len(rigid)==len(actual_positions) and {i for i,v in enumerate(rigid) if v}==head_members,'Source UV charts independently establish complete head/jaw/mouth membership')
require(len(mouth_members)>300 and mouth_members.issubset(head_members),'Every internal mouth point belongs to the rigid head')
corrupt=rigid[:];corrupt[min(mouth_members)]=0
rejects(lambda:require({i for i,v in enumerate(corrupt) if v}==head_members,'Dropped mouth transform'),'one internal mouth vertex weighted to Neck')
external_low=min(i for i in head_members if actual_positions[i][1]<.38 and i not in mouth_members)
corrupt=rigid[:];corrupt[external_low]=0
rejects(lambda:require({i for i,v in enumerate(corrupt) if v}==head_members,'Dropped exterior chin transform'),'low exterior head vertex weighted to Neck')

hand=(ROOT/'engine/include/fury/generated/player_body_hands.hpp').read_text();hr=json.loads((ASSETS/'player_body_hands_recipe.json').read_text())
def group(name):return sorted({i for names,corners in faces if name in names for i,_ in corners})
wrist=mean([morphed[i] for i in group('joint-l-hand')]);mid=mean([morphed[i] for i in group('joint-l-finger-3-1')]);index=mean([morphed[i] for i in group('joint-l-finger-2-1')]);pinky=mean([morphed[i] for i in group('joint-l-finger-5-1')])
down=unit(sub(mid,wrist));wide=sub(pinky,index);wide=unit(tuple(x-y*dot(wide,down) for x,y in zip(wide,down)));up=tuple(-x for x in down);normal=unit(cross(wide,up))
selected=[];selected_faces=[]
for face_id,(groups,corners) in enumerate(faces):
    if 'body' not in groups or not all(morphed[i][0]>wrist[0]*hr['source_side_fraction'] and dot(sub(morphed[i],wrist),down)>hr['source_wrist_plane_offset'] for i,_ in corners):continue
    for k in range(1,len(corners)-1):selected.append((corners[0],corners[k],corners[k+1]));selected_faces.append(face_id)
hids=sorted({i for t in selected for i,_ in t});hmap={v:i for i,v in enumerate(hids)}
huv=sorted({u for t in selected for _,u in t});huvmap={v:i for i,v in enumerate(huv)}
framed=[tuple(dot(sub(morphed[i],wrist),axis)*scale for axis in (wide,up,normal)) for i in hids]
def relax(p):
    x,y,z=p;t=max(0,min(1,(-y-hr['curl_start'])/hr['curl_span']));t=t*t*(3-2*t)
    return (x*(1-hr['curl_width']*t),y+hr['curl_lift']*t,z-hr['curl_depth']*t)
require(array(hand,'source_vertex_indices')==hids,'Independent hand source IDs')
require(array(hand,'source_frame_positions',3)==[tuple(f32(x) for x in p) for p in framed],'Source wrist-frame reconstruction')
hpos=array(hand,'positions',3)
require(hpos==[tuple(f32(x) for x in relax(p)) for p in framed],'Documented monotonic static curl exactly reconstructs hand')
require(1-1.5*hr['curl_lift']/hr['curl_span']>.8,'Static rest curl is monotonic, not a folding source warp')
require(array(hand,'near_indices')==[hmap[v] for t in selected for v,_ in t],'All Near hand triangles and source winding preserved')
require(array(hand,'near_uv_indices')==[huvmap[u] for t in selected for _,u in t] and array(hand,'source_uv_indices')==huv,'Every Near source hand texture corner preserved')
require(array(hand,'source_triangle_faces')==selected_faces,'Hand source face provenance')

def topology(indices):
    edges=collections.Counter();directions=collections.Counter();adj=collections.defaultdict(set)
    for k in range(0,len(indices),3):
        tri=indices[k:k+3];require(len(set(tri))==3,'Noncollapsed hand triangle')
        for a,b in zip(tri,tri[1:]+tri[:1]):edges[tuple(sorted((a,b)))]+=1;directions[tuple(sorted((a,b)))]+=1 if a<b else -1;adj[a].add(b);adj[b].add(a)
    require(all(n<=2 for n in edges.values()),'Manifold hand')
    require(all(directions[e]==0 for e,n in edges.items() if n==2),'Consistent hand winding')
    unseen=set(adj);stack=[unseen.pop()]
    while stack:
        for j in adj[stack.pop()]:
            if j in unseen:unseen.remove(j);stack.append(j)
    require(not unseen,'One connected hand')
    require(len(adj)-len(edges)+len(indices)//3==1,'Hand disk Euler characteristic')
    return {e for e,n in edges.items() if n==1}
near=array(hand,'near_indices');far=array(hand,'far_indices');near_boundary=topology(near);far_boundary=topology(far)
require(len(near_boundary)==20 and far_boundary==near_boundary,'Far preserves complete twenty-edge wrist attachment')
require(len(far)//3<=hr['far_triangles'],'Bounded Far hand')
corrupt=far[3:];rejects(lambda:require(topology(corrupt)==near_boundary,'Missing Far triangle'),'Far boundary')
corrupt=near[:];corrupt[1],corrupt[2]=corrupt[2],corrupt[1];rejects(lambda:topology(corrupt),'hand winding')
corrupt=list(hpos);corrupt[0]=(corrupt[0][0]+.002,*corrupt[0][1:]);rejects(lambda:require(corrupt==[tuple(f32(x) for x in relax(p)) for p in framed],'Altered hand fit'),'hand correspondence')
print(json.dumps({'pinned_inputs':len(manifest['shared_files']),'head_vertices':len(actual_positions),'head_triangles':len(actual_indices)//3,'head_source_corners':len(actual_uv_indices),'hand_vertices':len(hpos),'hand_near_triangles':len(near)//3,'hand_far_triangles':len(far)//3,'wrist_boundary_edges':len(near_boundary),'negative_controls':8,'rigid_head_source_vertices':len(head_members),'complete_mouth_source_vertices':len(mouth_members),'status':'passed'},indent=2))
