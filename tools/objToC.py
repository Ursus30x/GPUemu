#!/usr/bin/env python3

# WHY DOES THIS SCRIPT EXISTS
# I've created this script for ease of debugging without implementing obj loader in UEFI
# This is a simple stopgap measure and probably will be deleted when no longer needed

import sys

def parse_obj(filename):
    vertices = []
    edges = set()
    
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith('v '):
                # Parse vertex: v x y z
                parts = line.split()
                x, y, z = float(parts[1]), float(parts[2]), float(parts[3])
                vertices.append((x, y, z))
            
            elif line.startswith('f '):
                # Parse face: f v1 v2 v3 ...
                parts = line.split()[1:]
                # Extract vertex indices (handle formats like "v/vt/vn")
                indices = []
                for p in parts:
                    idx = int(p.split('/')[0]) - 1  # OBJ uses 1-based indexing
                    indices.append(idx)
                
                # Create edges from face (connect consecutive vertices)
                for i in range(len(indices)):
                    v1 = indices[i]
                    v2 = indices[(i + 1) % len(indices)]
                    # Add edge (sorted to avoid duplicates)
                    edges.add(tuple(sorted([v1, v2])))
    
    return vertices, list(edges)

def generate_colors(count):
    """Generate rainbow colors for vertices"""
    colors = []
    for i in range(count):
        hue = (i * 360 // count) % 360
        if hue < 60:
            r, g, b = 255, int(hue * 4.25), 0
        elif hue < 120:
            r, g, b = int(255 - (hue - 60) * 4.25), 255, 0
        elif hue < 180:
            r, g, b = 0, 255, int((hue - 120) * 4.25)
        elif hue < 240:
            r, g, b = 0, int(255 - (hue - 180) * 4.25), 255
        elif hue < 300:
            r, g, b = int((hue - 240) * 4.25), 0, 255
        else:
            r, g, b = 255, 0, int(255 - (hue - 300) * 4.25)
        
        colors.append(0xFF000000 | (r << 16) | (g << 8) | b)
    return colors

def output_c_code(vertices, edges):
    colors = generate_colors(len(vertices))
    
    print(f"#define MODEL_VERT_SIZE {len(vertices)}")
    print(f"#define MODEL_EDGE_SIZE {len(edges)}")
    print("")
    
    print("Vec3 model_vertices[] = {",end="")
    for i, (x, y, z) in enumerate(vertices):
        color = colors[i]
        print(f"{{{x:7.3f},{y:7.3f},{z:7.3f},0x{color:08X}}},", end="")
    print("};")
    print()
    
    print("Edge model_edges[] = {",end="")
    for i, (v1, v2) in enumerate(edges):
        comma = "," if i < len(edges) - 1 else ""
        print(f"{{{v1},{v2}}}{comma}", end="")
    print("};")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 convert_model.py model.obj")
        sys.exit(1)
    
    filename = sys.argv[1]
    vertices, edges = parse_obj(filename)
    
    print(f"// Converted from {filename}")
    print(f"// Vertices: {len(vertices)}, Edges: {len(edges)}")
    print()
    output_c_code(vertices, edges)