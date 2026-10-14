#!/usr/bin/env python3
"""Independent finite/topology audit of Cass's actual runtime diagnostic dump.

Run fury_character_cass_tests --dump PREFIX first. This does not create a mesh,
repair source geometry or retouch images. Coordinates are welded only after a
true neighboring-bucket distance check; UV/shading corners remain in the source.
"""
import argparse, collections, hashlib, json, math, struct
from pathlib import Path

REGIONS = [(0,0,1024,64),(1024,0,512,32),(1024,512,256,32),
           (1280,512,256,32),(1024,768,256,32),(1280,768,256,32),
           (0,1024,256,32),(256,1024,256,32),(512,1024,256,32),
           (768,1024,256,32),(1024,1024,256,32),(1280,1024,256,32),
           (0,1280,256,32)]
NAMES = ['Face','Jacket','Hair','Skin','Shirt','Pants','Leather','Eyes',
         'Rubber','Metal','HairStrands','HairCrop','HairShortStrands']
def surface(uv):
    for i,(x,y,size,gutter) in enumerate(REGIONS):
        if all((start+gutter+.5)/1536-1e-7 <= value <=
               (start+size-gutter-.5)/1536+1e-7 for start,value in zip((x,y),uv)):
            return NAMES[i]
    raise AssertionError('UV outside actual atlas content rectangles: '+str(uv))
def subtract(a,b): return tuple(x-y for x,y in zip(a,b))
def dot(a,b): return sum(x*y for x,y in zip(a,b))
def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def close_color(a,b): return max(abs(x-y)for x,y in zip(a,b))<1e-6

def topology(vertices, triangles, ids, height):
    points=[]; bins=collections.defaultdict(list)
    tolerance=1e-7; bucket=4e-7
    def weld(p):
        p=tuple(x/height for x in p)
        key=tuple(math.floor(x/bucket)for x in p)
        for a in (-1,0,1):
            for b in (-1,0,1):
                for c in (-1,0,1):
                    for i in bins.get((key[0]+a,key[1]+b,key[2]+c),()):
                        d=subtract(p,points[i])
                        if dot(d,d)<=tolerance*tolerance:return i
        i=len(points);points.append(p);bins[key].append(i);return i
    edges=collections.defaultdict(lambda:[0,0]); graph=collections.defaultdict(set)
    for number in ids:
        t=tuple(weld(vertices[i][:3])for i in triangles[number])
        assert len(set(t))==3, ('Welding would collapse a real triangle',number)
        for a,b in zip(t,t[1:]+t[:1]):
            e=edges[tuple(sorted((a,b)))];e[0]+=1;e[1]+=1 if a<b else -1
            graph[a].add(b);graph[b].add(a)
    def components(g):
        unseen=set(g); result=[]
        while unseen:
            todo=[unseen.pop()];count=0
            while todo:
                a=todo.pop();count+=1
                for b in g[a]:
                    if b in unseen:unseen.remove(b);todo.append(b)
            result.append(count)
        return sorted(result,reverse=True)
    boundary=collections.defaultdict(set)
    for (a,b),(count,orientation) in edges.items():
        if count==1:boundary[a].add(b);boundary[b].add(a)
    result={'points':len(points),'triangles':len(ids),'edges':len(edges),
            'connected_component_sizes':components(graph),
            'boundary_edges':sum(c[0]==1 for c in edges.values()),
            'boundary_loop_sizes':components(boundary),
            'branched_boundary_vertices':sum(len(v)!=2 for v in boundary.values()),
            'nonmanifold_edges':sum(c[0]>2 for c in edges.values()),
            'inconsistent_shared_edges':sum(c[0]==2 and c[1]!=0 for c in edges.values()),
            'euler_characteristic':len(points)-len(edges)+len(ids)}
    assert not result['branched_boundary_vertices'] and not result['nonmanifold_edges'] and not result['inconsistent_shared_edges'],result
    return result

def audit(path,height,lod):
    data=path.read_bytes(); nv,ni=struct.unpack_from('<II',data)
    assert len(data)==8+nv*64+ni*4 and ni%3==0
    vertices=list(struct.iter_unpack('<16f',data[8:8+nv*64]))
    triangles=list(struct.iter_unpack('<III',data[8+nv*64:]))
    surfaces=[surface(v[9:11])for v in vertices]
    for v in vertices:
        assert all(math.isfinite(x)for x in v)
        assert abs(math.sqrt(dot(v[3:6],v[3:6]))-1)<.0003
        assert all(0<=x<=1 for x in v[6:9])
    groups=collections.defaultdict(list);reversed_faces=[];tiny=[]
    for i,t in enumerate(triangles):
        assert len(set(t))==3 and max(t)<nv
        vs=[vertices[j]for j in t];s=surfaces[t[0]]
        assert all(surfaces[j]==s for j in t)
        p=[v[:3]for v in vs];n=cross(subtract(p[1],p[0]),subtract(p[2],p[0]))
        if dot(n,n)<1e-20:tiny.append(i)
        normals=tuple(sum(v[3+k]for v in vs)for k in range(3))
        if dot(n,normals)<=0:reversed_faces.append(i)
        lo=min(v[1]for v in vs)/height;hi=max(v[1]for v in vs)/height
        if s=='Skin' and hi<0:groups['left_hand'if sum(v[0]for v in vs)<0 else'right_hand'].append(i)
        if s=='Shirt'and lo>.24:groups['scarf'].append(i)
        if s=='HairCrop':groups['opaque_scalp'].append(i)
        if s=='Rubber'and lo>.4:groups['glasses_frames'].append(i)
        if s=='Eyes'and (close_color(vs[0][6:9],(.029,.039,.041))or close_color(vs[0][6:9],tuple(x*.82 for x in (.029,.039,.041)))):groups['lenses'].append(i)
        if s=='Jacket':groups['jacket'].append(i)
    assert not tiny,('Degenerate geometry',tiny[:20])
    assert not reversed_faces,('Nonpositive winding/shading agreement',reversed_faces[:30],len(reversed_faces))
    topologies={name:topology(vertices,triangles,ids,height)for name,ids in groups.items()}
    for name in ['left_hand','right_hand']:
        t=topologies[name];assert len(t['connected_component_sizes'])==1 and t['boundary_edges']==20 and len(t['boundary_loop_sizes'])==1 and t['euler_characteristic']==1,t
    t=topologies['scarf'];assert len(t['connected_component_sizes'])==2 and t['boundary_edges']==0 and t['euler_characteristic']==2,t
    t=topologies['lenses'];assert len(t['connected_component_sizes'])==2 and t['boundary_edges']==0 and t['euler_characteristic']==4,t
    t=topologies['glasses_frames'];assert len(t['connected_component_sizes'])==11 and t['boundary_edges']==0 and t['euler_characteristic']==18,t
    t=topologies['opaque_scalp'];assert len(t['connected_component_sizes'])==1 and len(t['boundary_loop_sizes'])==1 and t['euler_characteristic']==1,t
    assert len(triangles)<=(40000 if lod=='near'else 16000)
    return {'dump_sha256':hashlib.sha256(data).hexdigest(),'vertices':nv,'triangles':len(triangles),'height':height,'nonpositive_normal_agreement':len(reversed_faces),'degenerate_triangles':len(tiny),'topologies':topologies}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--mesh-prefix',type=Path,required=True);p.add_argument('--height',type=float,default=1.78);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    result={lod:audit(Path(str(a.mesh_prefix)+'-'+lod+'.bin'),a.height,lod)for lod in ['near','far']}
    a.output.write_text(json.dumps({'audit':'actual_runtime_dump','result':'passed','lods':result},indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
