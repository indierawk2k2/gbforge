"""Quest campaign model: a graph of opponents with staged challenges.

A QuestGraph is a linear (for now) ladder of Professors. Each
professor gates progress behind up to three challenge stages —
timed, puzzle set, battle — bracketed by dialogue pages. All of it
is data: the runtime quest driver walks the generated tables and
delegates each stage to the host game's mode runners.

Dialogue slot order matches the legacy DLG_* indices:
  0 meet, 1 timed-pre, 2 timed-win, 3 timed-lose,
  4 puzzle-pre, 5 puzzle-win, 6 puzzle-lose,
  7 battle-pre, 8 battle-win, 9 battle-lose.
Empty strings skip the page (the headmaster has no timed stage).
"""

DLG_SLOTS = 10


class Professor:
    def __init__(self, name, target_tile, time_limit_sec, battle_target,
                 spell_reward, dialogue, portrait=None, title="Professor"):
        """name: display last name; target_tile: numeric tile id the
        timed stage must create (also the transmute cap for stages);
        time_limit_sec: 0 = no timed stage; battle_target: knowledge
        goal for the battle exam; spell_reward: 1-based ability id
        granted on victory; dialogue: list of 10 strings (DLG order);
        portrait: portrait sheet slot (defaults to ladder position)."""
        if len(dialogue) != DLG_SLOTS:
            raise ValueError(f"{name}: dialogue needs {DLG_SLOTS} slots, "
                             f"got {len(dialogue)}")
        self.name = name
        self.title = title
        self.target_tile = target_tile
        self.time_limit_sec = time_limit_sec
        self.battle_target = battle_target
        self.spell_reward = spell_reward
        self.dialogue = list(dialogue)
        self.portrait = portrait

    @property
    def has_timed_stage(self):
        return self.time_limit_sec > 0


class QuestGraph:
    def __init__(self, professors, intro_slides=()):
        self.professors = list(professors)
        self.intro_slides = list(intro_slides)
        for i, p in enumerate(self.professors):
            if p.portrait is None:
                p.portrait = i
