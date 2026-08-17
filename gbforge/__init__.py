"""gbforge — thick SDK, thin runtime, for GBC puzzle games.

Rich Python classes describe a game (board rules, animations,
overlays, menus, abilities); build-time codegen emits GBDK C against a
small hand-written runtime. the private flagship game was the first definition;
nothing in this package may import from games/ (that's the generality
guarantee, enforced by test_no_game_imports).
"""

__version__ = "0.1.0"
