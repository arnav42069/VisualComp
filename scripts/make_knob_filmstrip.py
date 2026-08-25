#!/usr/bin/env python3
"""
Azazel Audio — rotary knob filmstrip generator.

Renders a vertical filmstrip of a dark-mode skeuomorphic knob: spun brushed
aluminium cap, raised slate bezel, ambient occlusion, and a micro-etched
indicator groove lit with the house accent orange.

ARCHITECTURE — a polar field compositor.
----------------------------------------
One supersampled float32 grid carries `r` and `theta` per pixel, in a
resolution-independent "128-unit" space (the frame is always 128 units wide, no
matter the output size). Every layer is an analytic mask over that field:
annuli are smoothstep bands on `r`, bevels and rims are f(r)*cos(theta - light),
the indicator is a distance field around the pointer axis.

The stack is split by REFERENCE FRAME, which is what makes the elevation read as
real rather than as stacked translucent ellipses:

  WORLD SPACE (baked once, shared by every frame) — anchored to a fixed light:
    L1 ambient occlusion       L2 bezel ring        L3 cast body shadow
    L4a cap cylindrical light + sheen lobes         L5 cap chamfer + specular rim

  OBJECT SPACE (re-rendered per frame at theta - theta_frame):
    L4b spun grain             L6 etched indicator + LED + bloom

So the shadow and every reflection stay put while only the cap texture and the
pointer turn. That is enforced by construction, not by discipline -- and
`--verify` proves it numerically (every pixel outside the cap face is
byte-identical across all 31 frames).

Compositing is done in PREMULTIPLIED float32 and downsampled to the output size
only at the very end, then unpremultiplied. Downsampling straight RGBA is what
produces the dark fringe around cheap knob art on transparent backgrounds.
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

# ---------------------------------------------------------------------------
# Geometry, in 128-unit frame space (centre = 0, frame edge = +/-64).
# Sized so the widest ambient shadow still clears the frame edge: nothing
# clips, and adjacent frames in the strip can never bleed into each other.
# ---------------------------------------------------------------------------
R_AO_CORE   = 46.0   # ambient occlusion stays solid out to here...
R_AO_EDGE   = 58.5   # ...then falls to nothing here
R_AO_CLIP   = 59.0   # hard clip; nothing may exist beyond this
AO_OFFSET_Y = 2.0    # shadow drop, in units

R_BEZ_OUT   = 54.0   # raised bezel ring, outer
R_BEZ_IN    = 46.0   # raised bezel ring, inner -> valley begins
R_CAP_OUT   = 43.0   # knob body silhouette
R_CHAMF     = 40.0   # cap face ends / chamfer begins

PTR_R0      = 14.0   # indicator groove, inner end
PTR_R1      = 38.0   # indicator groove, outer end
PTR_DOT_R   = 34.5   # LED dot centre, along the pointer
PTR_HALF_W  = 1.20   # groove half-width

SWEEP_DEG   = 270.0  # -135 .. +135

# Materials. The bezel is cool slate; the cap is warm-neutral aluminium. The
# small hue separation is what stops the two metals reading as one flat disc.
TINT_BEZEL  = np.array([0.962, 0.972, 0.990], dtype=np.float32)
TINT_CAP    = np.array([1.000, 0.988, 0.962], dtype=np.float32)

ACCENT_HEX  = "ff7a1f"   # Theme::accent
CHASSIS_HEX = "1d1d1b"   # Theme::bg  (the real VisualComp chassis)
FACEPLATE   = "1a1a1a"   # the slate faceplate specified for review


# ---------------------------------------------------------------------------
# Small numeric helpers
# ---------------------------------------------------------------------------

def hex_rgb(s: str) -> np.ndarray:
    s = s.lstrip("#")
    return np.array([int(s[i:i + 2], 16) / 255.0 for i in (0, 2, 4)], dtype=np.float32)


def smoothstep(e0: float, e1: float, x: np.ndarray) -> np.ndarray:
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return (t * t * (3.0 - 2.0 * t)).astype(np.float32)


def disc(r: np.ndarray, radius: float, aa: float) -> np.ndarray:
    """Soft-edged filled circle mask."""
    return (1.0 - smoothstep(radius - aa, radius + aa, r)).astype(np.float32)


def band(r: np.ndarray, r0: float, r1: float, aa: float) -> np.ndarray:
    """Soft-edged annulus mask."""
    return (smoothstep(r0 - aa, r0 + aa, r) *
            (1.0 - smoothstep(r1 - aa, r1 + aa, r))).astype(np.float32)


def over(dst_pm: np.ndarray, dst_a: np.ndarray,
         src_pm: np.ndarray, src_a: np.ndarray):
    """Porter-Duff `over`, premultiplied."""
    inv = (1.0 - src_a)[..., None]
    return src_pm + dst_pm * inv, src_a + dst_a * (1.0 - src_a)


def spun_noise(n: int, sigma: float, rng: np.random.Generator) -> np.ndarray:
    """1-D noise on a circle, gaussian-smoothed via FFT so it wraps seamlessly.

    A wrapped kernel matters here: a plain filter would leave a visible seam at
    theta = pi that would rotate around the cap and give the whole illusion away.
    """
    v = rng.standard_normal(n)
    k = np.fft.rfftfreq(n) * n
    g = np.exp(-0.5 * (2.0 * np.pi * k * sigma / n) ** 2)
    v = np.fft.irfft(np.fft.rfft(v) * g, n)
    return (v / (v.std() + 1e-9)).astype(np.float32)


def sample_ring(noise: np.ndarray, theta: np.ndarray) -> np.ndarray:
    """Linearly-interpolated lookup of circular noise at arbitrary angles."""
    n = noise.shape[0]
    idx = (theta / (2.0 * np.pi)) % 1.0 * n
    i0 = np.floor(idx).astype(np.int32)
    f = (idx - i0).astype(np.float32)
    i1 = (i0 + 1) % n
    i0 %= n
    return noise[i0] * (1.0 - f) + noise[i1] * f


# ---------------------------------------------------------------------------
# The polar field
# ---------------------------------------------------------------------------

class Field:
    """Resolution-independent polar sample grid, in 128-unit frame space."""

    def __init__(self, res: int):
        self.res = res
        u = ((np.arange(res, dtype=np.float32) + 0.5) / res * 128.0 - 64.0)
        self.x = np.broadcast_to(u[None, :], (res, res)).astype(np.float32)
        self.y = np.broadcast_to(u[:, None], (res, res)).astype(np.float32)
        self.r = np.sqrt(self.x ** 2 + self.y ** 2).astype(np.float32)
        # 0 = up, positive = clockwise -> direction (sin t, -cos t).
        # Same convention as AzazelLookAndFeel::drawRotarySlider's sa/ca.
        self.theta = np.arctan2(self.x, -self.y).astype(np.float32)
        self.aa = 128.0 / res * 1.4   # analytic edge width, in units


# ---------------------------------------------------------------------------
# WORLD-SPACE bake — everything anchored to the fixed light
# ---------------------------------------------------------------------------

class WorldBake:
    """Layers that must NOT rotate. Computed once, reused by all frames."""

    def __init__(self, f: Field, light_deg: float):
        self.f = f
        lt = math.radians(light_deg)
        # Unit vector pointing TOWARD the light, in screen coords.
        self.lux = math.sin(lt)
        self.luy = -math.cos(lt)

        # cos(theta - light): +1 facing the light, -1 facing away.
        self.ndotl = np.cos(f.theta - lt).astype(np.float32)
        self.lit = np.clip(self.ndotl, 0.0, None)
        self.shade = np.clip(-self.ndotl, 0.0, None)
        # Projection onto the light axis, normalised over the cap face.
        self.u = ((f.x * self.lux + f.y * self.luy) / R_CHAMF).astype(np.float32)

        self._bake_base()
        self._bake_cap_light()

    # -- L1 + L2 + L3 -------------------------------------------------------
    def _bake_base(self):
        f, aa = self.f, self.f.aa
        r = f.r

        pm = np.zeros((f.res, f.res, 3), dtype=np.float32)
        a = np.zeros((f.res, f.res), dtype=np.float32)

        # ---- L1  Ambient occlusion -------------------------------------
        # Wide and diffuse, dropped 2 units, so the knob reads as physically
        # detached from the faceplate rather than stamped onto it.
        ao_r = np.sqrt(f.x ** 2 + (f.y - AO_OFFSET_Y) ** 2)
        ao = 0.52 * (1.0 - smoothstep(R_AO_CORE, R_AO_EDGE, ao_r)) ** 1.5
        ao += 0.34 * (1.0 - smoothstep(50.0, 55.5, ao_r)) ** 1.2
        ao *= disc(r, R_AO_CLIP, 0.8)          # guarantee the bounds contract
        ao = np.clip(ao, 0.0, 0.86)
        pm, a = over(pm, a, np.zeros_like(pm), ao)   # pure black, premultiplied

        # ---- L2  Bezel ring --------------------------------------------
        # Drawn as a solid disc out to R_BEZ_OUT (the valley is just its dark
        # inner region) so there can be no transparent seam under the cap.
        t = np.clip((r - R_BEZ_IN) / (R_BEZ_OUT - R_BEZ_IN), 0.0, 1.0)

        flat = 0.100 + 0.048 * t + 0.052 * self.lit ** 1.4
        # Outer chamfer faces outward: lit on the upper arc.
        outer = 0.118 + 0.300 * self.lit ** 1.5 - 0.060 * self.shade
        # Inner chamfer faces inward, so its shading INVERTS. That flip is the
        # single strongest cue that the ring is raised and not painted on.
        inner = 0.118 + 0.220 * self.shade ** 1.6 - 0.055 * self.lit

        m_outer = smoothstep(51.0, 53.4, r)
        m_inner = 1.0 - smoothstep(46.6, 49.0, r)
        lum = flat * (1.0 - m_outer) * (1.0 - m_inner) + outer * m_outer + inner * m_inner

        # Sharp top highlight / dark bottom lip on the outer edge. Same
        # sub-pixel problem as the cap's chamfer: at 1 unit wide this
        # disappeared entirely once filtered down to knob size, taking the
        # ring's only metallic cue with it. Widened, and given a floor so the
        # ring is still described on its shaded side.
        edge = band(r, 51.9, R_BEZ_OUT, aa)
        lum += 0.44 * (0.20 + 0.80 * self.lit ** 3.4) * edge
        lum -= 0.075 * self.shade ** 2.0 * edge

        # The valley the cap sits in.
        valley = 1.0 - smoothstep(R_BEZ_IN - 0.6, R_BEZ_IN + 0.9, r)
        lum = lum * (1.0 - valley) + 0.040 * valley

        bez_rgb = np.clip(lum, 0.0, 1.6)[..., None] * TINT_BEZEL
        bez_a = disc(r, R_BEZ_OUT, aa)
        pm, a = over(pm, a, bez_rgb * bez_a[..., None], bez_a)

        # ---- L3  Cast body shadow ---------------------------------------
        # The cap throws a crisp, short shadow into the valley and just onto
        # the bezel's flat top, offset away from the light.
        sx = f.x + 1.7 * self.lux
        sy = f.y + 1.7 * self.luy
        sh_r = np.sqrt(sx ** 2 + sy ** 2)
        sh = 0.72 * (1.0 - smoothstep(40.0, 47.5, sh_r)) ** 1.3
        sh += 0.52 * (1.0 - smoothstep(42.0, 44.8, sh_r))
        sh *= disc(r, 49.5, 1.6) * bez_a
        sh = np.clip(sh, 0.0, 0.90)
        pm, a = over(pm, a, np.zeros((f.res, f.res, 3), dtype=np.float32), sh)

        self.base_pm, self.base_a = pm.astype(np.float32), a.astype(np.float32)

    # -- L4a + L5 -----------------------------------------------------------
    def _bake_cap_light(self):
        f, aa = self.f, self.f.aa
        r = f.r
        u = self.u

        # Body light along the light axis. Deliberately SHALLOW: this is a
        # flat machined cap, not a dome. A wide luminance range across the face
        # is exactly what makes a knob read as a soft rubber button at the
        # ~78px the plugin actually draws it, and the silhouette should be
        # defined by the chamfer below rather than by shading the face out to
        # nothing on its far side.
        lum = 0.200 + 0.070 * u + 0.034 * u ** 3

        # Metal's characteristic double highlight: a broad forward sheen plus
        # a weaker opposing lobe where the environment wraps around.
        lum += 0.072 * np.exp(-((u - 0.42) / 0.34) ** 2)
        lum += 0.040 * np.exp(-((u + 0.62) / 0.26) ** 2)

        # Anisotropic sheen arc. Circumferential brushing smears the specular
        # ALONG the brush direction, so a spun cap shows a broad bright arc at
        # mid-radius rather than a compact round hotspot. This is the single
        # cue that separates "spun metal" from "dark plastic".
        lum += 0.058 * np.exp(-((r - 24.0) / 16.0) ** 2) * \
            (0.5 + 0.5 * self.ndotl) ** 1.8

        # Self-occlusion just inside the rim on the shaded side.
        lum -= 0.036 * self.shade ** 1.5 * smoothstep(28.0, R_CHAMF, r)

        # Faint concentric lathe rings — the memory of the turning tool.
        lum += 0.0045 * np.sin(r * 3.1) * smoothstep(4.0, 14.0, r) * \
            (1.0 - smoothstep(34.0, R_CHAMF, r))

        face = 1.0 - smoothstep(R_CHAMF - 1.2, R_CHAMF + 0.4, r)

        # ---- L5  Chamfer + specular rim ---------------------------------
        # A machined chamfer catches light around its WHOLE circumference --
        # brightest toward the light, but never absent. That floor is what
        # stops the cap dissolving into the bezel on its shaded side, which is
        # what made it read as a dome rather than a cylinder. The rim band is
        # also wider than the 1 unit it started at: 1 unit is 0.6px once the
        # 192px master is filtered down to 78, i.e. gone.
        ch_lum = 0.145 + 0.190 * self.lit ** 1.5 + 0.050 * self.shade ** 2.0
        rim = band(r, 41.4, R_CAP_OUT, aa)
        ch_lum += 0.42 * (0.26 + 0.74 * self.lit ** 3.0) * rim
        ch_lum -= 0.055 * self.shade ** 1.2 * band(r, R_CHAMF, 41.4, aa)

        self.cap_lum = (lum * face + ch_lum * (1.0 - face)).astype(np.float32)
        self.cap_alpha = disc(r, R_CAP_OUT, aa).astype(np.float32)

        # Grain amplitude profile. Tapers out by r=37 — a few units clear of
        # R_CHAMF, because the 4x Lanczos downsample has ~3 output px of
        # support and would otherwise smear object-space energy onto the rim.
        # That margin is what lets --verify assert byte-identical world layers.
        amp = smoothstep(0.0, 6.0, r) * (1.0 - smoothstep(32.0, 37.0, r))
        # Grain is more visible where the surface is lit; barely there in shade.
        amp *= 0.55 + 0.75 * np.clip(u, 0.0, 1.0)
        self.grain_profile = amp.astype(np.float32)


# ---------------------------------------------------------------------------
# OBJECT-SPACE layers — the only things that turn
# ---------------------------------------------------------------------------

class Spinner:
    """Spun grain + etched indicator, evaluated at theta - theta_frame."""

    # A spun (lathe-turned) finish is CONCENTRIC: the tool marks are circles,
    # so brightness varies along RADIUS and is near-constant along theta. Noise
    # in theta instead of r gives a radial sunburst, which is the wrong metal.
    R_SAMPLES = 1536
    R_SPAN = 48.0

    def __init__(self, f: Field, w: WorldBake, accent: np.ndarray,
                 brush: float, seed: int):
        self.f, self.w, self.accent, self.brush = f, w, accent, brush
        rng = np.random.default_rng(seed)
        # Three octaves of concentric tool marks. Weighted hard toward the fine
        # end: too much low-frequency energy and the cap stops reading as
        # machined and starts reading as scuffed or dirty.
        self.n_micro = spun_noise(self.R_SAMPLES, 8.0, rng)    # ~0.6u period
        self.n_fine = spun_noise(self.R_SAMPLES, 20.0, rng)    # ~1.6u period
        self.n_broad = spun_noise(self.R_SAMPLES, 64.0, rng)   # slow tonal drift
        # Circumferential wobble — real turning lines are not perfect circles.
        # This is also what makes the rotation visible at all: without it the
        # texture would be exactly rotation-invariant. Kept well under the fine
        # octave's period so the marks still read as concentric.
        self.n_wobble = spun_noise(512, 3.0, rng)
        # Slow variation in which arcs catch the light.
        self.n_slow = spun_noise(256, 4.0, rng)

    def grain(self, theta_obj: np.ndarray) -> np.ndarray:
        wob = 0.35 * sample_ring(self.n_wobble, theta_obj)
        idx = np.clip((self.f.r + wob) / self.R_SPAN, 0.0, 1.0) * (self.R_SAMPLES - 1)
        i0 = idx.astype(np.int32)
        fr = (idx - i0).astype(np.float32)
        i1 = np.minimum(i0 + 1, self.R_SAMPLES - 1)

        def oct_(n):
            return n[i0] * (1.0 - fr) + n[i1] * fr

        g = 0.55 * oct_(self.n_fine) + 0.25 * oct_(self.n_micro)
        g += 0.14 * oct_(self.n_broad)
        g += 0.06 * sample_ring(self.n_slow, theta_obj)
        return (g * self.brush).astype(np.float32)

    def indicator(self, theta_f: float):
        """Etched groove + emissive LED. Returns (lum_delta, rgb, alpha, bloom)."""
        f, aa = self.f, self.f.aa
        c, s = math.cos(theta_f), math.sin(theta_f)
        # Rotate the sample point into object space; the pointer then always
        # lies along -y, so its distance field is trivial and exact.
        ox = f.x * c + f.y * s
        oy = -f.x * s + f.y * c
        along_pos = -oy
        d = np.abs(ox)

        along = smoothstep(PTR_R0 - 0.7, PTR_R0 + 0.7, along_pos) * \
            (1.0 - smoothstep(PTR_R1 - 0.7, PTR_R1 + 0.7, along_pos))
        groove = along * (1.0 - smoothstep(PTR_HALF_W - 0.35, PTR_HALF_W + 0.35, d))

        # The light direction, carried into object space, decides which inner
        # wall of the cut catches light. Without this the groove looks printed.
        llx = self.w.lux * c + self.w.luy * s
        wall_p = along * band(ox, PTR_HALF_W - 0.75, PTR_HALF_W + 0.20, aa)
        wall_n = along * band(-ox, PTR_HALF_W - 0.75, PTR_HALF_W + 0.20, aa)

        lum_d = -0.085 * groove
        lum_d += 0.30 * max(0.0, -llx) * wall_p
        lum_d += 0.30 * max(0.0, llx) * wall_n

        # Emissive core + the LED dot near the rim.
        core = along * (1.0 - smoothstep(0.40, 0.90, d))
        dot_d = np.sqrt(ox ** 2 + (along_pos - PTR_DOT_R) ** 2)
        dot = 1.0 - smoothstep(1.95, 2.75, dot_d)
        emis = np.clip(core * 0.92 + dot, 0.0, 1.0)

        hot = np.clip(dot * 0.85 + core * 0.15, 0.0, 1.0)[..., None]
        rgb = self.accent * (1.0 - hot) + np.array([1.0, 0.88, 0.74], np.float32) * hot

        # Bloom is purely additive and clipped inside the cap face, so it can
        # never touch the silhouette's alpha — no glowing halo, and the rim
        # stays byte-identical across frames.
        glow = np.exp(-np.maximum(d - 0.9, 0.0) ** 2 / 5.0) * along
        glow = np.maximum(glow, np.exp(-(dot_d ** 2) / 26.0))
        glow *= 1.0 - smoothstep(32.0, 36.5, f.r)
        bloom = (glow * 0.30)[..., None] * self.accent

        return lum_d.astype(np.float32), rgb.astype(np.float32), \
            emis.astype(np.float32), bloom.astype(np.float32)


# ---------------------------------------------------------------------------
# Frame assembly
# ---------------------------------------------------------------------------

def render_frame(f: Field, w: WorldBake, sp: Spinner, theta_f: float):
    theta_obj = (f.theta - theta_f).astype(np.float32)

    lum = w.cap_lum + sp.grain(theta_obj) * w.grain_profile
    lum_d, emis_rgb, emis_a, bloom = sp.indicator(theta_f)
    lum = lum + lum_d

    rgb = np.clip(lum, 0.0, 2.0)[..., None] * TINT_CAP
    rgb = rgb * (1.0 - emis_a[..., None]) + emis_rgb * emis_a[..., None]
    rgb = np.clip(rgb + bloom, 0.0, 1.0)

    ca = w.cap_alpha
    pm, a = over(w.base_pm, w.base_a, rgb * ca[..., None], ca)
    return pm, a


def downsample(pm: np.ndarray, a: np.ndarray, out: int) -> np.ndarray:
    """Premultiplied Lanczos resize, then unpremultiply. Order is the point."""
    def rs(arr: np.ndarray) -> np.ndarray:
        im = Image.fromarray(np.ascontiguousarray(arr, dtype=np.float32))
        return np.asarray(im.resize((out, out), Image.LANCZOS), dtype=np.float32)

    chans = [rs(pm[..., i]) for i in range(3)]
    ao = np.clip(rs(a), 0.0, 1.0)

    rgb_pm = np.clip(np.stack(chans, axis=-1), 0.0, 1.0)
    rgb_pm = np.minimum(rgb_pm, ao[..., None])          # kill Lanczos overshoot
    safe = np.maximum(ao, 1e-6)[..., None]
    rgb = np.where(ao[..., None] > 1e-4, rgb_pm / safe, 0.0)

    frame = np.concatenate([rgb, ao[..., None]], axis=-1)
    return np.clip(np.rint(frame * 255.0), 0, 255).astype(np.uint8)


def build_strip(size: int, frames: int, ss: int, light_deg: float,
                accent: np.ndarray, brush: float, seed: int) -> np.ndarray:
    res = size * ss
    f = Field(res)
    w = WorldBake(f, light_deg)
    sp = Spinner(f, w, accent, brush, seed)

    half = math.radians(SWEEP_DEG * 0.5)
    strip = np.zeros((size * frames, size, 4), dtype=np.uint8)
    for i in range(frames):
        t = i / (frames - 1)
        theta_f = -half + 2.0 * half * t
        pm, a = render_frame(f, w, sp, theta_f)
        strip[i * size:(i + 1) * size] = downsample(pm, a, size)
        print(f"\r  frame {i + 1:2d}/{frames}  ({math.degrees(theta_f):+7.2f} deg)",
              end="", flush=True)
    print()
    return strip


# ---------------------------------------------------------------------------
# Verification — prove the spec instead of eyeballing it
# ---------------------------------------------------------------------------

def verify(strip: np.ndarray, size: int, frames: int) -> bool:
    ok = True

    def check(name: str, cond: bool, detail: str = ""):
        nonlocal ok
        ok &= cond
        print(f"  [{'PASS' if cond else 'FAIL'}] {name}" + (f"  {detail}" if detail else ""))

    fr = [strip[i * size:(i + 1) * size].astype(np.float32) for i in range(frames)]
    sc = size / 128.0
    u = (np.arange(size, dtype=np.float32) + 0.5) / size * 128.0 - 64.0
    X = np.broadcast_to(u[None, :], (size, size))
    Y = np.broadcast_to(u[:, None], (size, size))
    Rr = np.sqrt(X ** 2 + Y ** 2)
    Th = np.arctan2(X, -Y)

    # 1. Strip geometry
    check("strip is exactly %dx%d" % (size, size * frames),
          strip.shape == (size * frames, size, 4), str(strip.shape))

    # 2. Bounds — nothing outside the reserved radius, so frames cannot bleed
    outside = Rr > R_AO_CLIP
    worst = max(float(f[..., 3][outside].max()) for f in fr)
    check("no alpha beyond r=59 (no inter-frame bleed)", worst <= 2.0,
          f"max alpha {worst:.1f}/255")

    # 3. THE non-rotation proof, in two parts.
    #    (a) Outside the knob body — AO, bezel, valley, cast shadow — nothing
    #        object-space is within reach of the resample kernel, so these must
    #        be byte-identical, full stop.
    outer = Rr > (R_CAP_OUT + 0.5)
    ref = fr[0][outer]
    d_outer = max(float(np.abs(f[outer] - ref).max()) for f in fr)
    check("shadow + bezel byte-identical across all frames", d_outer == 0.0,
          f"max deviation {d_outer:.0f}/255 over {int(outer.sum())} px")

    #    (b) The chamfer and specular rim are world-space too, but they sit
    #        within the ~3px Lanczos support of the indicator, so 1 LSB of
    #        resampling bleed is expected. Anything more would mean the
    #        lighting itself is turning.
    rim_z = (Rr > (R_CHAMF + 0.5)) & (Rr <= (R_CAP_OUT + 0.5))
    ref = fr[0][rim_z]
    d_rim = max(float(np.abs(f[rim_z] - ref).max()) for f in fr)
    check("chamfer + specular rim do not rotate", d_rim <= 1.0,
          f"max deviation {d_rim:.0f}/255 over {int(rim_z.sum())} px")

    # 4. THE rotation proof: cap grain must track the frame angle exactly.
    #    A spun finish is *deliberately* near rotation-invariant, so the signal
    #    is faint: correlating a single ring sits right at the pixel-arc
    #    resolution floor (~1.9 deg at r=30). Resample the whole cap face into
    #    polar (r, theta) and stack the correlations over every radius instead.
    #    The theta high-pass removes the static cylindrical lighting, which
    #    would otherwise dominate and pull every lag toward zero.
    # nr scales with output size: the finest grain octave has a ~0.6u
    # period, so a fixed radial bin count undersamples it (and aliases
    # into the measurement) at larger masters.
    nr, nt = int(48 * max(1.0, sc)), 2880
    rad = np.linspace(11.0, 35.0, nr)
    ang = np.linspace(-np.pi, np.pi, nt, endpoint=False)
    fx = (np.sin(ang)[None, :] * rad[:, None]) * sc + size / 2.0 - 0.5
    fy = (-np.cos(ang)[None, :] * rad[:, None]) * sc + size / 2.0 - 0.5
    x0, y0 = np.floor(fx).astype(int), np.floor(fy).astype(int)
    tx, ty = fx - x0, fy - y0
    # Gaussian theta high-pass, corner pinned in CYCLES PER TURN so it stays
    # put when nt changes (writing it against rfftfreq() alone silently moves
    # the filter with the bin count).
    #
    # The corner matters more than it looks. Its job is to kill the static
    # cylindrical lighting, which is a 1-cycle-per-turn pattern; 12 suppresses
    # that to 0.3% while leaving the grain's own angular structure intact. Set
    # it at 46 -- as this originally was -- and it eats the grain too, leaving
    # the estimator working on residue where the moving gate below is the
    # strongest thing in the frame. That biases every step +0.024 deg, which
    # hides inside the per-step tolerance and then accumulates coherently into
    # a false 1.4 deg error over 60 steps. Measured sweeps of the corner:
    #     corner  46    20    12     8     3
    #     bias  +.024 +.011 -.003 -.016 -.091   (deg/step)
    hp = 1.0 - np.exp(-0.5 * (np.arange(nt // 2 + 1) / 12.0) ** 2)

    # The indicator has to be gated out. It rotates, but its shading does not:
    # the fixed light picks out one inner wall of the cut, so the stripe's
    # profile changes shape as it turns and drags the correlation peak by up to
    # ~1/3 of its width (1.8 deg). Gating in OBJECT space removes the same
    # sector from every frame, so the comparison stays a rigid rotation.
    def polar_of(fi):
        g = fr[fi][..., :3].mean(axis=2).astype(np.float64)
        v = (g[y0, x0] * (1 - tx) * (1 - ty) + g[y0, x0 + 1] * tx * (1 - ty) +
             g[y0 + 1, x0] * (1 - tx) * ty + g[y0 + 1, x0 + 1] * tx * ty)
        v = v - v.mean(axis=1, keepdims=True)
        tf = math.radians(-SWEEP_DEG * 0.5 + SWEEP_DEG * fi / (frames - 1))
        d = np.degrees(np.abs((ang - tf + np.pi) % (2 * np.pi) - np.pi))
        v = v * np.clip((d - 8.0) / 6.0, 0.0, 1.0)[None, :]
        return np.fft.irfft(np.fft.rfft(v, axis=1) * hp, nt, axis=1)

    # Correlate ADJACENT frames rather than everything against frame 0. Two
    # reasons: neighbouring frames are near-identical so the peak is
    # unambiguous (a 135 deg comparison at 128px is weak enough that the global
    # argmax can hop to a spurious peak), and it tests every individual 9 deg
    # step instead of only the endpoints. Accumulating them then re-proves the
    # full sweep.
    # Rolling pair, not a list of all 61 spectra: at nt=5760 the full set is
    # ~200 MB, and only the previous frame is ever needed.
    step = SWEEP_DEG / (frames - 1)
    max_err, total = 0.0, 0.0
    prev = np.fft.rfft(polar_of(0), axis=1)
    for i in range(1, frames):
        cur = np.fft.rfft(polar_of(i), axis=1)
        cc = np.fft.irfft(cur * np.conj(prev), nt, axis=1).sum(axis=0)
        prev = cur
        k = int(np.argmax(cc))
        a0, a1, a2 = cc[(k - 1) % nt], cc[k], cc[(k + 1) % nt]
        den = a0 - 2 * a1 + a2
        kf = k + (0.5 * (a0 - a2) / den if den != 0 else 0.0)
        meas = (kf * 360.0 / nt + 180.0) % 360.0 - 180.0
        total += meas
        max_err = max(max_err, abs(meas - step))
    # Tolerances. Measured, for the record: the 192x61 plugin master lands at
    # 0.029 deg/step, the 256x31 concept master at 0.089, the 128x31 at 0.197 --
    # a coarser step measured off fewer pixels of weaker grain is simply a
    # harder measurement, and all three are the same analytic rotation.
    check(f"cap grain rotates {step:.0f} deg per frame", max_err <= 0.25,
          f"max step error {max_err:.3f} deg")

    # Relative, not absolute. The frame angles are analytic -- build_strip()
    # renders frame i at exactly -135 + 270*i/(n-1) deg -- so what this check is
    # really for is catching a coding error: a flipped sign, a wrong sweep
    # constant, grain that ended up sampled in world space (which would peak the
    # correlation at zero lag). All of those are off by tens of percent. What it
    # must NOT do is fail on the estimator's own residual shrinkage, which grows
    # as the master gets smaller and the grain weaker: correlating heavily
    # interpolated 128px content pulls the peak toward zero lag by ~0.06 deg per
    # 9 deg step. Expressed in degrees that would read as a 1.9 deg "error" in
    # the total and fail an asset that is exactly right; expressed as a fraction
    # it is 0.7%, comfortably inside a bound that still rejects every real fault.
    rel = abs(total / SWEEP_DEG - 1.0)
    check("accumulated grain rotation is the full sweep", rel <= 0.01,
          f"{total:.2f} deg over {frames - 1} steps "
          f"(want {SWEEP_DEG:.0f}, off by {rel * 100:.2f}%)")

    half = SWEEP_DEG * 0.5

    # 5. Pointer angle mapping: -135 .. +135, linear, centre frame dead vertical.
    #    Threshold isolates the emissive accent from the cap's own warm tint.
    def accent_mask(fi):
        f = fr[fi]
        return np.clip(f[..., 0] - f[..., 2] - 30.0, 0.0, None) * (Rr < R_CHAMF)

    def pointer_angle(fi):
        h = accent_mask(fi)
        return math.degrees(math.atan2(float((h * np.sin(Th)).sum()),
                                       float((h * np.cos(Th)).sum())))

    errs = [abs(pointer_angle(i) - (-half + SWEEP_DEG * (i / (frames - 1))))
            for i in range(frames)]
    check("pointer maps -135 .. +135 linearly", max(errs) <= 0.35,
          f"max error {max(errs):.3f} deg")

    # Geometry only — binarised. The groove's SHADING is deliberately
    # asymmetric (the fixed light picks out one inner wall of the cut), so a
    # pixel-exact colour mirror would be testing the wrong property.
    def core(fi):
        f = fr[fi]
        return ((f[..., 0] - f[..., 2]) > 120.0) & (Rr < R_CHAMF)

    mid = frames // 2
    n_mid = int((core(mid) ^ core(mid)[:, ::-1]).sum())
    check("centre frame pointer is exactly vertical", n_mid <= 2,
          f"{n_mid} px differ from its mirror (of {int(core(mid).sum())})")

    n_end = int((core(0) ^ core(frames - 1)[:, ::-1]).sum())
    check("first/last frame pointers mirror (+/-135)", n_end <= 4,
          f"{n_end} px differ (of {int(core(0).sum())})")

    # 6. No dark fringe — the tell-tale of an unpremultiplied downsample
    a = strip[..., 3:4].astype(np.float32) / 255.0
    onwhite = strip[..., :3].astype(np.float32) * a + 255.0 * (1.0 - a)
    ring = np.tile(Rr >= 60.0, (frames, 1))
    darkest = float(onwhite.mean(axis=2)[ring].min())
    check("no dark fringe over white", darkest >= 253.0,
          f"darkest {darkest:.1f}/255")

    return ok


# ---------------------------------------------------------------------------
# Review sheet
# ---------------------------------------------------------------------------

def contact_sheet(strip: np.ndarray, size: int, frames: int, out: Path):
    picks = [0, 5, 10, 15, 20, 25, 30] if frames == 31 else \
        list(range(0, frames, max(1, frames // 7)))[:7]
    backdrops = [(FACEPLATE, "faceplate #1a1a1a"),
                 (CHASSIS_HEX, "chassis #1d1d1b (Theme::bg)"),
                 ("ffffff", "white (fringe check)")]

    gap, pad, lab = 14, 22, 20
    w = pad * 2 + len(picks) * size + (len(picks) - 1) * gap
    h = pad + len(backdrops) * (size + lab + gap)
    sheet = Image.new("RGB", (w, h), (12, 12, 12))
    d = ImageDraw.Draw(sheet)

    for row, (bg, name) in enumerate(backdrops):
        y = pad + row * (size + lab + gap)
        d.text((pad, y), name, fill=(150, 150, 150))
        col = tuple(int(bg[i:i + 2], 16) for i in (0, 2, 4))
        for j, fi in enumerate(picks):
            x = pad + j * (size + gap)
            tile = Image.new("RGB", (size, size), col)
            frame = Image.fromarray(strip[fi * size:(fi + 1) * size], "RGBA")
            tile.paste(frame, (0, 0), frame)
            sheet.paste(tile, (x, y + lab))
            if row == len(backdrops) - 1:
                deg = -SWEEP_DEG / 2 + SWEEP_DEG * fi / (frames - 1)
                d.text((x, y + lab + size + 3), f"{fi:02d}  {deg:+.0f}deg",
                       fill=(120, 120, 120))
    sheet.save(out)


# ---------------------------------------------------------------------------

def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--size", type=int, default=128, help="frame size in px")
    ap.add_argument("--frames", type=int, default=31, help="frames in the strip")
    ap.add_argument("--ss", type=int, default=4, help="supersample factor")
    ap.add_argument("--light-deg", type=float, default=315.0,
                    help="light direction, 0=up, cw (default 315 = upper-left)")
    ap.add_argument("--accent", default=ACCENT_HEX, help="indicator colour")
    ap.add_argument("--brush", type=float, default=0.030,
                    help="spun grain amplitude, in luminance")
    ap.add_argument("--seed", type=int, default=20260825)
    ap.add_argument("--out", type=Path, default=None)
    ap.add_argument("--sheet", type=Path, default=None, help="contact sheet path")
    ap.add_argument("--no-verify", action="store_true")
    a = ap.parse_args(argv)

    out = a.out or Path(f"resources/knob-azazel-{a.size}x{a.frames}.png")
    out.parent.mkdir(parents=True, exist_ok=True)

    print(f"Rendering {a.frames} frames at {a.size}px "
          f"({a.size * a.ss}px internal, {a.ss}x supersample)")
    strip = build_strip(a.size, a.frames, a.ss, a.light_deg,
                        hex_rgb(a.accent), a.brush, a.seed)
    Image.fromarray(strip, "RGBA").save(out)
    print(f"  -> {out}  ({a.size}x{a.size * a.frames})")

    if a.sheet:
        contact_sheet(strip, a.size, a.frames, a.sheet)
        print(f"  -> {a.sheet}")

    if not a.no_verify:
        print("Verifying:")
        if not verify(strip, a.size, a.frames):
            print("VERIFICATION FAILED")
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
