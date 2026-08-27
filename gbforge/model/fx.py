"""Full-screen cast effects.

A CastFx names one of the runtime fx interpreter's primitives
(gbforge/runtime/fx.c) plus its parameters. Games attach one to an
Ability (`cast_fx=`); the cast cutscene shows the ability's name
overlay while the effect runs, then the board reveal applies the
ability's result.

The primitives are the classic GB/GBC full-screen repertoire:
palette flashes and tints (CRAM rewrites), screen shakes (SCX/SCY
jitter), and per-scanline raster effects (wave/shear via an LY
busy-poll — no LYC ISR, so HOME stays untouched).
"""

from dataclasses import dataclass

FX_NONE = 0
FX_FLASH = 1      # a=pulses, b=0 white / 1 black
FX_SHAKE = 2      # a=amplitude px (decays to 0)
FX_WAVE = 3       # a=amplitude px, b=phase speed
FX_SHEAR = 4      # a=amplitude px, b=split scanline/8 (tile row)
FX_TINT = 5       # a,b,c = target r,g,b (0-31); ramps in and out
FX_GRAYFADE = 6   # desaturate toward per-color luma and back
FX_TRIFADE = 7    # fire/water/earth tint pulses in sequence


@dataclass
class CastFx:
    kind: int
    frames: int = 32
    a: int = 0
    b: int = 0
    c: int = 0

    @classmethod
    def flash(cls, pulses=2, black=False, frames=24):
        return cls(FX_FLASH, frames, pulses, 1 if black else 0)

    @classmethod
    def shake(cls, amplitude=6, frames=36):
        return cls(FX_SHAKE, frames, amplitude)

    @classmethod
    def wave(cls, amplitude=4, speed=3, frames=48):
        return cls(FX_WAVE, frames, amplitude, speed)

    @classmethod
    def shear(cls, amplitude=8, split_row=9, frames=40):
        return cls(FX_SHEAR, frames, amplitude, split_row)

    @classmethod
    def tint(cls, r, g, b, frames=40):
        return cls(FX_TINT, frames, r, g, b)

    @classmethod
    def grayfade(cls, frames=48):
        return cls(FX_GRAYFADE, frames)

    @classmethod
    def trifade(cls, frames=60):
        return cls(FX_TRIFADE, frames)
