"""THE match-engine reference implementation.

A line-for-line faithful port of the original hand-written match + gravity
+ spells.c resource accounting. This is the single source of truth the
three legacy implementations collapse into: match.c (validated against
this via the transcript oracle), puzzle.c's sim_* (never ported), and
the private sims in scripts/gen_puzzles.py / craft_puzzles.py (ported
to import this module).

Proof of equivalence: `python3 -m gbforge.engine.sim --transcript`
must print the same fingerprint as `make -C tests transcript`
(stored in harness/golden/corpus/engine_transcript.json).

Semantics deliberately preserved bug-for-bug — divergence from match.c
is an error here, even where match.c looks quirky.
"""

W = H = 8

# Tile types (tiles_data.h)
EMPTY = 0
FIRE, WATER, EARTH = 1, 2, 3
BRONZE, SILVER, GOLD, PLAT, EMERALD, RUBY, OBSID, AETHER = range(4, 12)
TILE_TYPE_COUNT = 12

MANNA_CAP = 99
KNOWLEDGE_CAP = 9999
MAX_TRANSMUTES = 8

_NEXT_TIER = {BRONZE: SILVER, SILVER: GOLD, GOLD: PLAT, PLAT: EMERALD,
              EMERALD: RUBY, RUBY: OBSID, OBSID: AETHER}

_KNOWLEDGE = {BRONZE: 1, SILVER: 3, GOLD: 10, PLAT: 25,
              EMERALD: 50, RUBY: 100, OBSID: 200, AETHER: 500}


def is_metal(t):
    return BRONZE <= t <= AETHER

def is_manna(t):
    return FIRE <= t <= EARTH


def manna_for_run(run):
    if run >= 6:
        return 10
    if run >= 5:
        return 7
    if run >= 4:
        return 5
    return run


class ItemEffects:
    """Zeroed active_effects — the transcript build's state. In-game
    defaults come from item_refresh_effects (chain_bonus_pct 50 etc.)."""
    manna_cap = 0
    manna_bonus_fire = 0
    manna_bonus_water = 0
    manna_bonus_earth = 0
    knowledge_pct_bonus = 0
    chain_bonus_pct = 0


class Engine:
    """Board + match state. Mirrors match.c's module globals."""

    def __init__(self, board=None, max_tier=AETHER, effects=None,
                 rng=None):
        self.board = [row[:] for row in board] if board else \
            [[EMPTY] * W for _ in range(H)]
        self.match_max_tier = max_tier
        self.effects = effects or ItemEffects()
        # rng(n) -> int in [0, n): only consumed by the 4-chain bonus
        # when chain_bonus_pct > 0 (match.c calls rand() first, but a
        # 0-pct roll can never pass, so behavior is rng-free then).
        self.rng = rng
        self.matched = [[0] * W for _ in range(H)]
        self.last_max_run = 0
        self.transmutes = []          # [(x, y)]
        self.bonus_transmute = None   # (x, y) or None
        # resources (spells.c)
        self.manna = {FIRE: 0, WATER: 0, EARTH: 0}
        self.knowledge = 0

    # ── spells.c ────────────────────────────────────────────────────

    def add_manna(self, tile_type, amount):
        cap = self.effects.manna_cap or MANNA_CAP
        if tile_type not in self.manna:
            return
        self.manna[tile_type] = min(cap, self.manna[tile_type] + amount)

    def add_knowledge(self, amount):
        self.knowledge = min(KNOWLEDGE_CAP, self.knowledge + amount)

    # ── match.c ─────────────────────────────────────────────────────

    def next_tier(self, t):
        nxt = _NEXT_TIER.get(t, EMPTY)
        if nxt == EMPTY or nxt > self.match_max_tier:
            return EMPTY
        return nxt

    def find(self):
        """match_find: mark straight runs >= 3; max_run also counts
        per-type matched totals (L/T shapes)."""
        self.matched = [[0] * W for _ in range(H)]
        self.last_max_run = 0
        found = False
        b = self.board

        for y in range(H):
            x = 0
            while x < W:
                t = b[y][x]
                if t == EMPTY:
                    x += 1
                    continue
                run = 1
                while x + run < W and b[y][x + run] == t:
                    run += 1
                if run >= 3:
                    for i in range(run):
                        self.matched[y][x + i] = 1
                    found = True
                    self.last_max_run = max(self.last_max_run, run)
                x += run

        for x in range(W):
            y = 0
            while y < H:
                t = b[y][x]
                if t == EMPTY:
                    y += 1
                    continue
                run = 1
                while y + run < H and b[y + run][x] == t:
                    run += 1
                if run >= 3:
                    for i in range(run):
                        self.matched[y + i][x] = 1
                    found = True
                    self.last_max_run = max(self.last_max_run, run)
                y += run

        if found:
            counts = [0] * TILE_TYPE_COUNT
            for y in range(H):
                for x in range(W):
                    if self.matched[y][x]:
                        counts[b[y][x]] += 1
            self.last_max_run = max(self.last_max_run, max(counts[1:]))
        return found

    def dirty_rows(self):
        d = 0
        for y in range(H):
            if any(self.matched[y]):
                d |= 1 << y
        return d

    def _award(self, tile_type, run):
        """award_match minus the float spawn (render side)."""
        e = self.effects
        if is_manna(tile_type):
            amount = manna_for_run(run)
            amount += {FIRE: e.manna_bonus_fire, WATER: e.manna_bonus_water,
                       EARTH: e.manna_bonus_earth}[tile_type]
            self.add_manna(tile_type, amount)
        elif is_metal(tile_type):
            kp = _KNOWLEDGE[tile_type] * run
            if e.knowledge_pct_bonus > 0:
                kp += (kp * e.knowledge_pct_bonus) // 100
            self.add_knowledge(kp)

    def _flood_manna(self, saved, awarded):
        """process_manna_matches: connected components of matched manna
        (4-neighborhood), each awarded once by total size."""
        for sy in range(H):
            for sx in range(W):
                if not self.matched[sy][sx] or awarded[sy][sx]:
                    continue
                t = saved[sy][sx]
                if not is_manna(t):
                    continue
                stack = [(sx, sy)]
                count = 0
                while stack:
                    x, y = stack.pop()
                    if awarded[y][x] or not self.matched[y][x] or \
                            saved[y][x] != t:
                        continue
                    awarded[y][x] = 1
                    count += 1
                    if x > 0:
                        stack.append((x - 1, y))
                    if x < W - 1:
                        stack.append((x + 1, y))
                    if y > 0:
                        stack.append((x, y - 1))
                    if y < H - 1:
                        stack.append((x, y + 1))
                if count >= 3:
                    self._award(t, count)

    def _try_4chain_bonus(self, metal_type):
        nxt = self.next_tier(metal_type)
        if nxt == EMPTY:
            return
        pct = self.effects.chain_bonus_pct
        if self.rng is None or pct == 0:
            return  # roll >= 0 always fails when pct == 0
        if self.rng(100) >= pct:
            return
        candidates = [(x, y) for y in range(H) for x in range(W)
                      if self.board[y][x] == metal_type]
        if not candidates:
            return
        x, y = candidates[self.rng(len(candidates))]
        self.board[y][x] = nxt
        self.bonus_transmute = (x, y)

    def _metal_run(self, saved, positions):
        """One directional matched-run of metal tiles: award + transmute.
        positions = [(x, y)] along the run in scan order."""
        t = saved[positions[0][1]][positions[0][0]]
        run = len(positions)
        nxt = self.next_tier(t)
        self._award(t, run)
        if nxt != EMPTY:
            if run >= 5:
                spots = (positions[1], positions[run - 2])
            else:
                spots = (positions[run >> 1],)
            for x, y in spots:
                self.board[y][x] = nxt
                if len(self.transmutes) < MAX_TRANSMUTES:
                    self.transmutes.append((x, y))
            if run == 4:
                self._try_4chain_bonus(t)

    def process(self):
        """match_process: clear matched, flood-award manna, run-scan
        metals for awards/transmutes. Returns group count."""
        self.transmutes = []
        self.bonus_transmute = None
        saved = [row[:] for row in self.board]
        count = 0

        for y in range(H):
            for x in range(W):
                if self.matched[y][x]:
                    self.board[y][x] = EMPTY

        awarded = [[0] * W for _ in range(H)]
        self._flood_manna(saved, awarded)

        # horizontal matched-runs of one saved type
        for y in range(H):
            x = 0
            while x < W:
                if not self.matched[y][x]:
                    x += 1
                    continue
                t = saved[y][x]
                run = 0
                while x + run < W and self.matched[y][x + run] and \
                        saved[y][x + run] == t:
                    run += 1
                if run >= 3:
                    if is_metal(t):
                        self._metal_run(saved,
                                        [(x + i, y) for i in range(run)])
                    count += 1
                x += run

        # vertical
        for x in range(W):
            y = 0
            while y < H:
                if not self.matched[y][x]:
                    y += 1
                    continue
                t = saved[y][x]
                run = 0
                while y + run < H and self.matched[y + run][x] and \
                        saved[y + run][x] == t:
                    run += 1
                if run >= 3:
                    if is_metal(t):
                        self._metal_run(saved,
                                        [(x, y + i) for i in range(run)])
                    count += 1
                y += run

        return count

    # ── board.c ─────────────────────────────────────────────────────

    def apply_gravity(self):
        moved = False
        b = self.board
        for x in range(W):
            write_y = H - 1
            for y in range(H - 1, -1, -1):
                if b[y][x] != EMPTY:
                    if y != write_y:
                        b[write_y][x] = b[y][x]
                        b[y][x] = EMPTY
                        moved = True
                    write_y -= 1
        return moved

    def swap(self, x1, y1, x2, y2):
        b = self.board
        b[y1][x1], b[y2][x2] = b[y2][x2], b[y1][x1]

    def has_legal_moves(self):
        """has_legal_moves: any adjacent swap creating a 3-run."""
        b = self.board

        def check_at(x, y):
            t = b[y][x]
            if t == EMPTY:
                return False
            n = 1
            i = x
            while i > 0 and b[y][i - 1] == t:
                n += 1
                i -= 1
            i = x
            while i < W - 1 and b[y][i + 1] == t:
                n += 1
                i += 1
            if n >= 3:
                return True
            n = 1
            i = y
            while i > 0 and b[i - 1][x] == t:
                n += 1
                i -= 1
            i = y
            while i < H - 1 and b[i + 1][x] == t:
                n += 1
                i += 1
            return n >= 3

        for y in range(H):
            for x in range(W):
                for dx, dy in ((1, 0), (0, 1)):
                    nx, ny = x + dx, y + dy
                    if nx >= W or ny >= H:
                        continue
                    self.swap(x, y, nx, ny)
                    hit = check_at(x, y) or check_at(nx, ny)
                    self.swap(x, y, nx, ny)
                    if hit:
                        return True
        return False


# ── transcript oracle (mirror of tests/engine_transcript.c) ─────────

def _transcript(boards=10000):
    fnv = 1469598103934665603
    MASK = (1 << 64) - 1

    def put(b):
        nonlocal fnv
        fnv = ((fnv ^ b) * 1099511628211) & MASK

    def put_board(e):
        for y in range(H):
            for x in range(W):
                put(e.board[y][x])

    total_passes = total_groups = 0
    for n in range(boards):
        state = (0x9E3779B9 ^ n) & 0xFFFFFFFF

        def xs():
            nonlocal state
            x = state
            x = (x ^ (x << 13)) & 0xFFFFFFFF
            x ^= x >> 17
            x = (x ^ (x << 5)) & 0xFFFFFFFF
            state = x
            return x

        board = []
        for _ in range(H):
            row = []
            for _ in range(W):
                r = xs()
                row.append(1 + r % 3 if r % 8 < 5 else 4 + (r >> 8) % 5)
            board.append(row)

        e = Engine(board)
        put_board(e)

        while e.find():
            total_passes += 1
            for y in range(H):
                for x in range(W):
                    put(e.matched[y][x])
            put(e.last_max_run)
            put(e.dirty_rows())

            total_groups += e.process()
            put(len(e.transmutes))
            put_board(e)
            put(e.manna[FIRE])
            put(e.manna[WATER])
            put(e.manna[EARTH])
            put(e.knowledge & 0xFF)
            put(e.knowledge >> 8)

            e.apply_gravity()
            put_board(e)

    print(f"boards={boards} passes={total_passes} groups={total_groups}")
    print(f"transcript_hash={fnv:016x}")
    return fnv


if __name__ == "__main__":
    import sys
    _transcript(10000 if "--transcript" in sys.argv else 200)
