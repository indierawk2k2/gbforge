"""Audio theme: per-event sound effects and per-scene music, as data.

The runtime raises a fixed vocabulary of events (EVENTS below — the
generated C enum shares this order). The theme maps each event to a
sound-effect NAME from the editor-owned res enum (the game's res/
sfx_data.h — gbforge reads that header at codegen, never writes it),
and each scene to a music track symbol. Unmapped events are silent;
scenes without a track keep whatever music is playing.

A completely different game maps the same event vocabulary onto its
own effect library — laser zaps instead of water splashes — without
touching the runtime.
"""

# Runtime event vocabulary. Order is the C enum — append only.
EVENTS = [
    # input / board interaction
    "cursor_move", "tile_select", "tile_deselect", "swap", "swap_fail",
    # matches (element = matched tile's element, size = run length)
    "match_3_fire", "match_3_water", "match_3_earth",
    "match_4_fire", "match_4_water", "match_4_earth",
    "match_5", "metal_match",
    # cascade depth (pass number during one resolve)
    "chain_2", "chain_3", "chain_4", "chain_5",
    # transmutation, by created tier (bronze .. aether)
    "transmute_bronze", "transmute_silver", "transmute_gold",
    "transmute_platinum", "transmute_emerald", "transmute_ruby",
    "transmute_obsidian", "transmute_aether",
    # board motion
    "fall", "refill",
    # abilities / spells
    "cast", "spell_cycle", "spell_purchase",
    # menus and full-screen UI
    "menu_move", "menu_confirm", "menu_cancel",
    "store_open", "store_close",
    # outcomes
    "win", "lose", "warning",
]

# Scene list = ["title"] + game mode names + these extras, in order.
EXTRA_SCENES = ["quest_hub", "dialogue", "store"]


class AudioTheme:
    def __init__(self, events=None, music=None):
        """events: {event_name: "SFX_*" name}; music: {scene: track
        symbol from the music res (e.g. "song_data") or None}."""
        self.events = dict(events or {})
        self.music = dict(music or {})
        for k in self.events:
            if k not in EVENTS:
                raise KeyError(f"unknown audio event {k!r}")
