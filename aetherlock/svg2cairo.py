import xml.etree.ElementTree as ET
import re
import sys

def parse_svg(filename):
    tree = ET.parse(filename)
    root = tree.getroot()
    paths = []
    for path in root.findall('.//{http://www.w3.org/2000/svg}path'):
        paths.append(path.attrib['d'])
    return paths

def generate_cairo(paths):
    print("void draw_vaxp_logo(cairo_t *cr, double cx, double cy, double scale) {")
    print("    cairo_save(cr);")
    print("    cairo_translate(cr, cx, cy);")
    print("    cairo_scale(cr, scale, scale);")
    # Apply the SVG transform: scale(1, -1) and translate(0, 1024)
    # But wait, we want the logo centered at 0,0! The viewBox is 1024x1024.
    # Center is 512, 512.
    print("    cairo_translate(cr, -512, -512);")
    print("    cairo_translate(cr, 0, 1024);")
    print("    cairo_scale(cr, 1.0, -1.0);")

    for d in paths:
        # Pad letters with space
        d = re.sub(r'([A-Za-z])', r' \1 ', d)
        tokens = d.split()
        
        i = 0
        while i < len(tokens):
            cmd = tokens[i]
            if cmd == 'M':
                x = float(tokens[i+1])
                y = float(tokens[i+2])
                print(f"    cairo_move_to(cr, {x}, {y});")
                i += 3
            elif cmd == 'c':
                i += 1
                while i < len(tokens) and not tokens[i].isalpha():
                    dx1 = float(tokens[i])
                    dy1 = float(tokens[i+1])
                    dx2 = float(tokens[i+2])
                    dy2 = float(tokens[i+3])
                    dx3 = float(tokens[i+4])
                    dy3 = float(tokens[i+5])
                    print(f"    cairo_rel_curve_to(cr, {dx1}, {dy1}, {dx2}, {dy2}, {dx3}, {dy3});")
                    i += 6
            elif cmd == 'l':
                i += 1
                while i < len(tokens) and not tokens[i].isalpha():
                    dx = float(tokens[i])
                    dy = float(tokens[i+1])
                    print(f"    cairo_rel_line_to(cr, {dx}, {dy});")
                    i += 2
            elif cmd == 'z':
                print("    cairo_close_path(cr);")
                i += 1
            else:
                print(f"    // UNKNOWN CMD {cmd}")
                i += 1
    
    print("    cairo_fill(cr);")
    print("    cairo_restore(cr);")
    print("}")

paths = parse_svg("logo.svg")
generate_cairo(paths)
