# -*- coding: utf-8 -*-
# 修改：许双博（基于你提供的脚本）
# 功能：将 OBJ 顶点与索引数据转为 C++ 可直接编译数组，
#       并在导出的 tempVertices 中直接包含法线 (XMFLOAT3)：
#         { XMFLOAT3(x,y,z), XMFLOAT3(nx,ny,nz), XMFLOAT4(r,g,b,a) }
# 时间：2025年10月（更新）

import os, sys, math, random

# ====== 配置：把 OBJ 文件名写在这里 ======
filename = "wang.obj"   # <-- 将 OBJ 文件名写到这里
# ========================================

if not os.path.exists(filename):
    print(f"No file: {filename}")
    sys.exit(1)

output_path = os.path.splitext(filename)[0] + "_output.txt"

positions = []   # list of (x,y,z) from 'v'
vn_list = []     # list of (nx,ny,nz) from 'vn' (if provided)
faces = []       # each face: list of tuples (v_idx, vt_idx or None, vn_idx or None)

# 读取 OBJ（支持 v, vt, vn, f 的常见格式）
with open(filename, "r", encoding="utf-8", errors="ignore") as f:
    for raw in f:
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("v "):
            parts = line.split()
            if len(parts) >= 4:
                x = float(parts[1]); y = float(parts[2]); z = float(parts[3])
                positions.append((x,y,z))
        elif line.startswith("vn "):
            parts = line.split()
            if len(parts) >= 4:
                nx = float(parts[1]); ny = float(parts[2]); nz = float(parts[3])
                # 归一化一下以防奇怪数据
                l = math.sqrt(nx*nx + ny*ny + nz*nz)
                if l == 0.0:
                    vn_list.append((0.0,0.0,1.0))
                else:
                    vn_list.append((nx/l, ny/l, nz/l))
        elif line.startswith("f "):
            parts = line[2:].strip().split()
            face = []
            for p in parts:
                v = vt = vn = None
                if "//" in p:
                    a,b = p.split("//")
                    v = int(a) - 1
                    vn = int(b) - 1
                else:
                    comps = p.split('/')
                    if len(comps) == 1:
                        v = int(comps[0]) - 1
                    elif len(comps) == 2:
                        v = int(comps[0]) - 1
                        vt = int(comps[1]) - 1 if comps[1] != '' else None
                    else:
                        v = int(comps[0]) - 1
                        vt = int(comps[1]) - 1 if comps[1] != '' else None
                        vn = int(comps[2]) - 1 if comps[2] != '' else None
                face.append((v, vt, vn))
            if len(face) >= 3:
                faces.append(face)

if len(positions) == 0 or len(faces) == 0:
    print("未解析到顶点或面，请检查 OBJ 文件")
    sys.exit(1)

# triangulate faces (fan) and build indices referencing original positions order
indices = []
# prepare accum normals per position (to compute averaged normals)
accum_normals = [ [0.0,0.0,0.0] for _ in positions ]
has_vn = len(vn_list) > 0

def sub(a,b): return (a[0]-b[0], a[1]-b[1], a[2]-b[2])
def cross(a,b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
def normalize(v):
    lx = v[0]*v[0] + v[1]*v[1] + v[2]*v[2]
    if lx <= 1e-12:
        return (0.0,0.0,1.0)
    l = math.sqrt(lx)
    return (v[0]/l, v[1]/l, v[2]/l)

# For color stability choose fixed seed (or remove to random every run)
random.seed(12345)

# create vertex color list in same order as positions to preserve original indexing behaviour
vertex_colors = [ (round(random.random(),2), round(random.random(),2), round(random.random(),2), 1.0) for _ in positions ]

for face in faces:
    # fan triangulation
    for i in range(1, len(face)-1):
        tri = (face[0], face[i], face[i+1])  # keeps original winding
        # compute face normal:
        need_geom = True
        for vt in tri:
            if vt[2] is not None and has_vn:
                need_geom = False
                break
        if need_geom:
            p0 = positions[tri[0][0]]; p1 = positions[tri[1][0]]; p2 = positions[tri[2][0]]
            e1 = sub(p1,p0); e2 = sub(p2,p0)
            fn = cross(e1,e2)
            fn = normalize(fn)
            # accumulate
            for vt in tri:
                idx = vt[0]
                accum_normals[idx][0] += fn[0]
                accum_normals[idx][1] += fn[1]
                accum_normals[idx][2] += fn[2]
        else:
            # if any vn used, prefer to add the referenced vn values per-vertex
            for vt in tri:
                idx = vt[0]
                if vt[2] is not None and vt[2] < len(vn_list):
                    vn = vn_list[vt[2]]
                    accum_normals[idx][0] += vn[0]
                    accum_normals[idx][1] += vn[1]
                    accum_normals[idx][2] += vn[2]
                else:
                    # fallback geometric normal
                    p0 = positions[tri[0][0]]; p1 = positions[tri[1][0]]; p2 = positions[tri[2][0]]
                    e1 = sub(p1,p0); e2 = sub(p2,p0)
                    fn = cross(e1,e2)
                    fn = normalize(fn)
                    accum_normals[idx][0] += fn[0]
                    accum_normals[idx][1] += fn[1]
                    accum_normals[idx][2] += fn[2]

        # append triangle indices in same style as original script (v2,v1,v0)
        i0 = tri[2][0]; i1 = tri[1][0]; i2 = tri[0][0]
        indices.extend([i0, i1, i2])

# finalize normals per position (normalize accumulation)
final_normals = []
for a in accum_normals:
    nx, ny, nz = a[0], a[1], a[2]
    fn = normalize((nx,ny,nz))
    final_normals.append(fn)

# ensure no zero normals
for i,fn in enumerate(final_normals):
    if abs(fn[0]) < 1e-9 and abs(fn[1]) < 1e-9 and abs(fn[2]) < 1e-9:
        final_normals[i] = (0.0, 0.0, 1.0)

# ----- 输出文件 -----
def fmt(x):
    # 保持与已有脚本类似的浮点文本格式（去掉末尾无意义零）
    return ("%.6f" % x).rstrip('0').rstrip('.') if abs(x) >= 1e-6 else "0.000000"

with open(output_path, "w", encoding="utf-8") as out:
    out.write("// ===== Generated C++ model data (with per-vertex normals embedded) =====\n")
    out.write("// Source OBJ: %s\n\n" % filename)

    # 顶点：position + normal + color（直接满足 XMFLOAT3, XMFLOAT3, XMFLOAT4）
    out.write("static const GameApp::VertexPosColor tempVertices[] = {\n")
    for pos, nrm, col in zip(positions, final_normals, vertex_colors):
        out.write("    { XMFLOAT3(%s, %s, %s), XMFLOAT3(%s, %s, %s), XMFLOAT4(%s, %s, %s, %s) },\n" % (
            fmt(pos[0]), fmt(pos[1]), fmt(pos[2]),
            fmt(nrm[0]), fmt(nrm[1]), fmt(nrm[2]),
            fmt(col[0]), fmt(col[1]), fmt(col[2]), fmt(col[3])
        ))
    out.write("};\n\n")

    # 仍然导出独立 tempNormals（可选，便于检查或兼容旧代码）
    # out.write("static const XMFLOAT3 tempNormals[] = {\n")
    # for n in final_normals:
    #     out.write("    XMFLOAT3(%s, %s, %s),\n" % (fmt(n[0]), fmt(n[1]), fmt(n[2])))
    # out.write("};\n\n")

    # indices
    out.write("static const WORD tempIndices[] = {\n")
    per_line = 12
    for i in range(0, len(indices), per_line):
        chunk = indices[i:i+per_line]
        out.write("    " + ", ".join(str(x) for x in chunk) + ("," if i+per_line < len(indices) else "") + "\n")
    out.write("};\n\n")

    out.write("// meta\n")
    out.write("// verticesCount = %d\n" % len(positions))
    out.write("// indexCount = %d\n" % len(indices))
    out.write("// NOTE: tempVertices 已包含法线 (pos, normal, color)。若渲染法线方向不对，可在渲染端反向法线或调整三角顺序。\n")

print(f"已生成: {output_path}")
print(f"顶点数: {len(positions)}, 索引数: {len(indices)}, 输出包含 tempNormals 与 tempVertices(包含法线)")
