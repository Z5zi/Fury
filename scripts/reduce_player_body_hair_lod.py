import bpy,json,sys
from pathlib import Path
r=Path(__file__).resolve().parents[1]/'third_party/player_body_hair';j=json.loads((r/'fit/fit.json').read_text())
keep=set(i for c in j['components'] if c['min'][1]>.37 for i in c['indices'])
ids=sorted(keep);remap={old:i for i,old in enumerate(ids)}
fs=[p for p in j['faces'] if p[0][0] in keep]
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
m=bpy.data.meshes.new('CC0_Player_fitted_scalp');m.from_pydata([j['vertices'][i] for i in ids],[],[[remap[i] for i,u in f] for f in fs]);m.update()
o=bpy.data.objects.new('CC0_Player_fitted_scalp',m);bpy.context.collection.objects.link(o);bpy.context.view_layer.objects.active=o;o.select_set(True)
uv=m.uv_layers.new(name='SourceUV')
for face,source in zip(m.polygons,fs):
 for li,(_,ui) in zip(face.loop_indices,source):uv.data[li].uv=j['uvs'][ui]
for poly in m.polygons:poly.use_smooth=True

def export(path):
 me=o.data;me.calc_loop_triangles();uv=me.uv_layers.active.data
 data={'positions':[list(v.co) for v in me.vertices],'triangles':[[[int(me.loops[l].vertex_index),list(uv[l].uv)] for l in t.loops] for t in me.loop_triangles]}
 path.write_text(json.dumps(data,separators=(',',':')))
 print(path,'vertices',len(data['positions']),'triangles',len(data['triangles']),flush=True)
export(r/'scalp-near.json')
mod=o.modifiers.new('Far geometry reduction','DECIMATE');mod.ratio=.30;mod.use_collapse_triangulate=True
bpy.ops.object.modifier_apply(modifier=mod.name)
export(r/'scalp-far.json')

