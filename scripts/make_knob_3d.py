"""Render an actual lathed 3D knob into the existing JUCE filmstrip.

Analytic ray intersections with a lathed solid minus 24 cylindrical grip cuts,
also exported as an OBJ mesh. Orthographic camera, geometric normals, area-light shadows,
and linear-light shading. No external renderer or runtime 3D engine required.
Run with the bundled Python (numpy and Pillow), from the repository root.
"""
from pathlib import Path
import argparse
import math
import numpy as np
from PIL import Image, ImageDraw
from make_knob_filmstrip import downsample

# (radius, height). Consecutive pairs form the surface of revolution.
# Dimensions are model units, with the panel at z=0. Closed solid geometry.
HEIGHT_EXTENSION = 35.09  # 49.3 * 1.30 = 64.09 total height; preserve base/bevel.
GRIP_BOTTOM_Z = 5.0
SHOULDER_Z = 22.0 + HEIGHT_EXTENSION
BEVEL_Z = 25.0 + HEIGHT_EXTENSION
TOP_Z = 29.0 + HEIGHT_EXTENSION
PROFILE = [(0, 0.5), (50, 0.5), (53, 2), (53, 3.5), (50, GRIP_BOTTOM_Z),
           (44, GRIP_BOTTOM_Z), (44, SHOULDER_Z), (43, BEVEL_Z), (38, TOP_Z), (0, TOP_Z)]
MATERIALS = [0, 0, 0, 0, 1, 1, 1, 2, 3, 1]
GRIP_COUNT, GRIP_CENTRE, GRIP_RADIUS = 24, 44.4, 2.8
TILT = math.radians(10.08)  # Previous 14.4-degree off-normal tilt reduced by 30%.
SCREEN_OFFSET = 3.0
ARC_RADIUS = 36.5
COLORS = np.array([[0.60, 0.64, 0.63], [0.055, 0.062, 0.062],
                   [0.15, 0.17, 0.17], [0.115, 0.13, 0.13]], np.float32)


def unit(v):
    return v / np.maximum(np.linalg.norm(v, axis=-1, keepdims=True), 1e-10)


def in_grip_cut(x, y, z, grip_angle=0.0):
    angle = grip_angle + np.round((np.arctan2(y, x)-grip_angle)*GRIP_COUNT/(2*math.pi))*(2*math.pi/GRIP_COUNT)
    distance2 = (x-GRIP_CENTRE*np.cos(angle))**2 + (y-GRIP_CENTRE*np.sin(angle))**2
    return (z > GRIP_BOTTOM_Z + 0.0001) & (distance2 < GRIP_RADIUS**2 - 0.0001)


def shell_radius(z):
    return np.where(z <= SHOULDER_Z, 44,
                    np.where(z <= BEVEL_Z, 44-(z-SHOULDER_Z)/3,
                             43-(z-BEVEL_Z)*1.25))


def intersect(origin, direction, normals=True, grip_angle=0.0):
    """Nearest positive intersection with a closed surface of revolution."""
    shape = origin.shape[:-1]
    nearest = np.full(shape, np.inf, np.float32)
    surface = np.full(shape, -1, np.int16)
    for i, ((r0, z0), (r1, z1)) in enumerate(zip(PROFILE, PROFILE[1:])):
        ox, oy, oz = origin[..., 0], origin[..., 1], origin[..., 2]
        dx, dy, dz = direction[..., 0], direction[..., 1], direction[..., 2]
        if z0 == z1:
            t = (z0 - oz) / dz
            radius2 = (ox + t * dx)**2 + (oy + t * dy)**2
            valid = ((radius2 >= min(r0, r1)**2) &
                     (radius2 <= max(r0, r1)**2) & (t > 0.0001))
            if i == 4:
                # Flat floors below the cuts close the exposed shoulder at z=5.
                valid |= ((radius2 <= r1*r1) & (t > 0.0001)
                          & in_grip_cut(ox+t*dx, oy+t*dy, np.full_like(oz, GRIP_BOTTOM_Z + 0.001), grip_angle))
            valid &= ~in_grip_cut(ox+t*dx, oy+t*dy, np.full_like(oz, z0), grip_angle)
            replace = valid & (t < nearest)
            nearest = np.where(replace, t, nearest)
            surface = np.where(replace, i, surface)
            continue
        slope = (r1 - r0) / (z1 - z0)
        local_r = r0 + slope * (oz - z0)
        qa = dx*dx + dy*dy - (slope*dz)**2
        qb = 2 * (ox*dx + oy*dy - local_r*slope*dz)
        qc = ox*ox + oy*oy - local_r*local_r
        det = qb*qb - 4*qa*qc
        root = np.sqrt(np.maximum(det, 0))
        for sign in (-1, 1):
            t = (-qb + sign * root) / (2 * qa)
            z = oz + t * dz
            valid = ((det >= 0) & (t > 0.0001) &
                     (z >= min(z0, z1)) & (z <= max(z0, z1)))
            valid &= ~in_grip_cut(ox+t*dx, oy+t*dy, z, grip_angle)
            replace = valid & (t < nearest)
            nearest = np.where(replace, t, nearest)
            surface = np.where(replace, i, surface)
    # Exposed concave faces of 24 cylindrical cuts: geometry, not painted stripes.
    cut_normal = np.zeros_like(origin) if normals else None
    qa = dx*dx + dy*dy
    for angle in np.linspace(0, 2*math.pi, GRIP_COUNT, endpoint=False) + grip_angle:
        ccx, ccy = GRIP_CENTRE*math.cos(angle), GRIP_CENTRE*math.sin(angle)
        lx, ly = ox-ccx, oy-ccy
        qb = 2*(lx*dx+ly*dy)
        qc = lx*lx+ly*ly-GRIP_RADIUS**2
        det = qb*qb-4*qa*qc
        for sign in (-1, 1):
            t = (-qb+sign*np.sqrt(np.maximum(det, 0))) / (2*qa)
            px, py, pz = ox+t*dx, oy+t*dy, oz+t*dz
            valid = ((det >= 0) & (t > 0.0001) & (pz >= GRIP_BOTTOM_Z) & (pz <= TOP_Z)
                     & (px*px+py*py <= shell_radius(pz)**2))
            replace = valid & (t < nearest)
            nearest = np.where(replace, t, nearest)
            surface = np.where(replace, 9, surface)
            if normals:
                n = np.stack((ccx-px, ccy-py, np.zeros_like(px)), axis=-1)
                cut_normal[replace] = unit(n)[replace]
    if not normals:
        return np.isfinite(nearest)
    hit = np.isfinite(nearest)
    point = origin + np.where(hit, nearest, 0)[..., None] * direction
    normal = np.zeros_like(origin)
    normal[surface == 9] = cut_normal[surface == 9]
    for i, ((r0, z0), (r1, z1)) in enumerate(zip(PROFILE, PROFILE[1:])):
        mask = surface == i
        if z0 == z1:
            normal[mask] = (0, 0, 1 if r1 < r0 else -1)
        else:
            slope = (r1-r0)/(z1-z0)
            n = np.stack((point[..., 0], point[..., 1],
                          -slope * np.hypot(point[..., 0], point[..., 1])), axis=-1)
            normal[mask] = unit(n)[mask]
    return hit, point, normal, surface


def render_base(size, ss, grip_angle=0.0):
    res = size * ss
    u = (np.arange(res, dtype=np.float32) + 0.5) * 128 / res - 64
    x, y = np.meshgrid(u, u)
    camera = np.array([0, math.sin(TILT), math.cos(TILT)], np.float32)
    down = np.array([0, math.cos(TILT), -math.sin(TILT)], np.float32)
    origin = np.zeros((res, res, 3), np.float32)
    origin[..., 0] = x
    origin += (y-SCREEN_OFFSET)[..., None] * down + camera * 400
    ray = -camera
    hit, point, normal, surface = intersect(origin, ray, grip_angle=grip_angle)
    assert not (hit[0].any() or hit[-1].any() or hit[:, 0].any() or hit[:, -1].any()), 'model clips frame'
    # Soft shadows come from actual occlusion of rays to an area light.
    light = unit(np.array([-0.55, -0.65, 1.35], np.float32))
    visibility = np.zeros_like(x)
    ground = origin + ((-origin[..., 2]) / ray[2])[..., None] * ray
    shading_point = np.where(hit[..., None], point, ground)
    shading_normal = np.where(hit[..., None], normal, np.array([0, 0, 1], np.float32))
    for angle in np.linspace(0, 2*math.pi, 12, endpoint=False):
        area_ray = unit(light + np.array([math.cos(angle)*0.13,
                                         math.sin(angle)*0.13, 0], np.float32))
        blocked = intersect(shading_point + shading_normal*0.025, area_ray, False, grip_angle)
        visibility += (~blocked).astype(np.float32) / 12
    material = np.take(np.array(MATERIALS), np.maximum(surface, 0))
    albedo = np.take(COLORS, material, axis=0)
    # Convert display color to linear light, then apply normal-based shading.
    albedo = albedo ** 2.2
    diffuse = np.maximum(np.sum(normal * light, axis=-1), 0)
    half = unit(light + camera)
    spec = np.maximum(np.sum(normal * half, axis=-1), 0)
    rough_exp = np.where(material == 0, 42, np.where(material == 2, 28, 65))
    spec = spec ** rough_exp * np.where(material == 0, 0.26, 0.055)
    rgb = albedo * (0.29 + 1.45*diffuse*visibility)[..., None]
    rgb += (spec * visibility)[..., None]
    rgb = np.maximum(rgb, 0) ** (1/2.2)
    # Darken only the concave grip walls, including their specular response.
    # Keep the top face, seat, body dimensions, and lighting unchanged.
    rgb[surface == 9] *= 0.72
    # A thin channel on the TOP face accepts the live JUCE value arc.
    radius = np.hypot(point[..., 0], point[..., 1])
    channel = hit & (surface == 8) & (np.abs(radius - ARC_RADIUS) < 0.65)
    rgb[channel] *= 0.48
    shadow_alpha = (1-visibility)*0.48
    # Small contact occlusion under the mounting seat, clipped with a smooth
    # frame-edge taper so the shadow can never become a rectangular tile.
    ground_r = np.hypot(ground[..., 0], ground[..., 1])
    contact = np.exp(-np.maximum(ground_r-50, 0)**2 / 36)*0.25
    shadow_alpha = 1-(1-shadow_alpha)*(1-contact)
    # Radial feather avoids a visible square shadow footprint on the panel,
    # especially now that the taller body casts a longer shadow.
    edge = np.clip((62-np.hypot(x, y))/10, 0, 1)
    shadow_alpha *= edge*edge*(3-2*edge)
    alpha = np.where(hit, 1, shadow_alpha)
    pm = np.where(hit[..., None], rgb, 0) * alpha[..., None]
    return pm, alpha, point, hit & (surface == 8)


def build(size=192, frames=61, ss=3):
    output, bases, cache = [], [], {}
    accent = np.array([1, 122/255, 31/255], np.float32)
    for i in range(frames):
        degrees = -135 + 270*i/(frames-1)
        angle = math.radians(degrees)
        # The identical ribs repeat every 15 degrees: only ten distinct
        # geometry/light bakes are needed for the standard 61-frame sweep.
        # Rotate geometry around Z, NOT the finished image or its light source.
        phase = round((degrees - 90) % (360/GRIP_COUNT), 8)
        if phase not in cache:
            print(f'Rendering rotating grip phase {phase:g} degrees', flush=True)
            cache[phase] = render_base(size, ss, math.radians(phase))
        pm, alpha, point, top = cache[phase]
        along = point[..., 0]*math.sin(angle)-point[..., 1]*math.cos(angle)
        across = point[..., 0]*math.cos(angle)+point[..., 1]*math.sin(angle)
        mark = top & (along >= 28) & (along <= 33.5) & (np.abs(across) < 0.85)
        marked = np.where(mark[..., None], accent, pm)
        output.append(downsample(marked, alpha, size))
        bases.append(downsample(pm, alpha, size))
    return np.concatenate(output), np.stack(bases)


def verify(strip, bases, size, frames):
    images = strip.reshape(frames, size, size, 4).astype(np.int16)
    # Compare each pointer against its own rotated-body bake.
    u = (np.arange(size)+0.5)*128/size-64
    x, sy = np.meshgrid(u, u)
    wy = (sy-SCREEN_OFFSET + TOP_Z*math.sin(TILT))/math.cos(TILT)
    errors = []
    for i, im in enumerate(images):
        weight = np.maximum(im[..., 0]-im[..., 2]-60, 0)
        assert weight.sum() > 0, 'missing marker'
        measured = math.degrees(math.atan2(float((weight*x).sum()), float((-weight*wy).sum())))
        errors.append(abs(measured-(-135+270*i/(frames-1))))
        angle = math.radians(-135+270*i/(frames-1))
        along = x*math.sin(angle)-wy*math.cos(angle)
        across = x*math.cos(angle)+wy*math.sin(angle)
        region = (along > 25) & (along < 37) & (np.abs(across) < 4)
        delta = np.abs(im-bases[i].astype(np.int16)).max(axis=-1)
        assert delta[~region].max() <= 1, 'pointer changes unrelated pixels'
    # Smooth face lighting remains fixed while sidewall geometry really moves.
    face = np.hypot(x, wy) < 25
    assert np.abs(bases.astype(np.int16)-bases[0].astype(np.int16))[:, face].max() <= 1, 'top lighting rotates'
    assert np.abs(bases[1].astype(np.int16)-bases[0].astype(np.int16)).max() > 8, 'grip is stationary'
    if frames == 61:
        assert np.array_equal(bases[0], bases[10]), '24-rib rotational periodicity'
    for degrees in (0, 4.5, 9, 13.5):
        a = math.radians(degrees)
        for z in (GRIP_BOTTOM_Z + 1, (GRIP_BOTTOM_Z + SHOULDER_Z)/2, SHOULDER_Z - 1):
            assert in_grip_cut(43*math.cos(a), 43*math.sin(a), z, a), 'groove does not follow rotation'
            assert not in_grip_cut(43*math.cos(a+math.pi/GRIP_COUNT), 43*math.sin(a+math.pi/GRIP_COUNT), z, a), 'rib incorrectly cut'
    assert max(errors) < 0.6, f'pointer mapping error: {max(errors)}'
    assert images[:, (0, -1), :, 3].max() == 0, 'vertical edge alpha'
    assert images[:, :, (0, -1), 3].max() == 0, 'horizontal edge alpha'
    print(f'PASS: rotating grip geometry, fixed top lighting, {frames} pointer angles (max error {max(errors):.3f} deg), transparent frame edges')


def export_model(path):
    """Editable closed mesh, identical profile to the ray-traced solid."""
    segments = 768
    lines = ['# VisualComp raised knob; Z is height above faceplate', 'o VisualComp_Knob_3D']
    rings = []
    vertex = 1
    mesh_profile = [PROFILE[0]]
    mesh_materials = []
    for j, ((r0, z0), (r1, z1)) in enumerate(zip(PROFILE, PROFILE[1:])):
        steps = 16 if j in (6, 7) else 1
        for step in range(1, steps+1):
            mix = step/steps
            mesh_profile.append((r0+(r1-r0)*mix, z0+(z1-z0)*mix))
            mesh_materials.append(MATERIALS[j])
    for ring_index, (radius, z) in enumerate(mesh_profile):
        count = 1 if radius == 0 else segments
        rings.append(list(range(vertex, vertex + count)))
        vertex += count
        for i in range(count):
            a = 2*math.pi*i/segments
            r = radius
            if ring_index >= 5 and radius > 0:
                delta = a-round(a*GRIP_COUNT/(2*math.pi))*(2*math.pi/GRIP_COUNT)
                discriminant = GRIP_RADIUS**2-(GRIP_CENTRE*math.sin(delta))**2
                if discriminant >= 0:
                    r = min(r, GRIP_CENTRE*math.cos(delta)-math.sqrt(discriminant))
            lines.append(f'v {r*math.cos(a):.6f} {r*math.sin(a):.6f} {z:.6f}')
    for j in range(len(mesh_profile)-1):
        lines.append('g ' + ['seat', 'shell', 'bevel', 'top'][mesh_materials[j]])
        for i in range(segments):
            lower, upper = rings[j], rings[j+1]
            if len(lower) == 1:
                lines.append(f'f {lower[0]} {upper[(i+1)%segments]} {upper[i]}')
            elif len(upper) == 1:
                lines.append(f'f {lower[i]} {lower[(i+1)%segments]} {upper[0]}')
            else:
                lines.append(f'f {lower[i]} {lower[(i+1)%segments]} {upper[(i+1)%segments]} {upper[i]}')
    path.write_text('\n'.join(lines)+'\n', encoding='utf-8')


def sheet(strip, size, path):
    picks = [0, 15, 30, 45, 60]
    canvas = Image.new('RGB', (5*150, 4*150), '#202320')
    draw = ImageDraw.Draw(canvas)
    for row, cell in enumerate([128, 78, 49, 45]):
        for col, frame in enumerate(picks):
            tile = Image.new('RGB', (150, 150), '#7b7e79' if row < 2 else '#292e28')
            art = Image.fromarray(strip[frame*size:(frame+1)*size]).resize((cell,cell), Image.Resampling.LANCZOS)
            tile.paste(art, ((150-cell)//2, (135-cell)//2), art)
            canvas.paste(tile, (col*150,row*150))
            draw.text((col*150+8,row*150+133), f'{cell}px / {frame/60:.0%}', fill='white')
    canvas.save(path)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--size', type=int, default=192)
    parser.add_argument('--ss', type=int, default=3)
    args = parser.parse_args()
    strip, base = build(args.size, 61, args.ss)
    verify(strip, base, args.size, 61)
    Image.fromarray(strip).save('resources/knob-azazel-192x61.png')
    sheet(strip, args.size, Path('resources/knob-contact-sheet.png'))
    export_model(Path('resources/knob-raised-3d.obj'))
    print('Saved 3D mesh, 61-frame filmstrip, and contact sheet.')
