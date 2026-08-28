# Retro

Every rule in [AGENTS.md](../AGENTS.md) came from something going
wrong. Read on its own, that file is a list of assertions with no why. This file has the reasons behind those rules.

Most of what follows happened in the parent project, a complete Game
Boy Color game, six months of work, from which this repository was
extracted. Six of the ten entries are failures. Sixty percent of the time, it fails every time. 😃

**The short version:**

1. If agents keep colliding, look at the code before the task scheduler.
2. A test that has never failed is not evidence that anything works.
3. A rule that asks you to remember something will eventually be forgotten.
4. "The tests pass" is a claim about the tests, not about what shipped.
5. Differential testing cannot see a feature that was never wired up.
6. Image comparison at a match threshold is blind to small, systematic errors.
7. A file half-owned by a tool is a file that tool will eventually truncate.
8. When automation loses on taste, move it into the tool rather than deleting it.
9. An equivalence oracle finds bugs in the old implementation too.
10. Slice work until no step's result has to be taken on trust.

---

## Architecture

### 1. The parallel task runner that made everything slower

By March I had a verification loop I trusted, so the bottleneck moved.
I could review results faster than I could feed work to an agent. So I
built a task runner: a board of tasks in a JSON file, slash commands to
create and dispatch them, and a script that opened a terminal tab per
task, each with its own agent session. Tasks marked ready could be
fanned out all at once, and each got its own git worktree so the
sessions could not overwrite each other's files.

**What I expected.** Four or five small fixes landing in an afternoon
instead of over two days.

**What happened.** They collided constantly. The worktrees kept the
*files* apart but not the *work*. Of the twelve tasks I ran this way,
four modified `store.c` and three modified `render.c` — an 824-line
file and a 2,249-line file. Merging them back was manual and
conflict-heavy, and the commit log from those days is half real work
and half cleaning up after the runner: orphaned task output, a broken
symlink, worktrees to reap by hand.

**Why.** A Game Boy game of this kind has one main loop and one
renderer, and nearly every task — a cursor fix, a menu tweak, a
palette timing change — is a small edit somewhere inside them. Nothing
about the code created a boundary that two workers could sit either
side of, so no amount of process could manufacture one.

**What I changed.** I stopped. The last parallel task landed at 15:59
on March 16; a replacement was committed at 20:31 the same evening — a
runner that reads a list and executes one item at a time. I worked that
way from then on and did not revisit the question.

What actually fixed it was not aimed at it. In August I split the game
into a declarative spec and a shared runtime, because agents kept
changing one game mode and leaving the other five behind. Once that
landed, editing a mode's rules and editing the renderer stopped being
edits to the same file — and parallel work became possible as a side
effect. It has been fine since.

**What I would tell someone else.** If your agents keep stepping on
each other, look at the code before you look at the scheduler.
Parallelism is a property of how the work decomposes. I spent three
days building the wrong fix, and I did not find the right one by
looking for it — it arrived as a by-product of solving a different
problem.

---

## Verification

### 2. The tests passed and the bugs were still there

Four things were visibly wrong in the game: a white flash between
gameplay and the win screen, stray zeros left on the countdown timer
in timed mode, the opponent's score flickering during battle
animations, and the spell panel going blank when a result card
appeared. I asked for each to be fixed and for a test to be written so
it would not come back.

**What I expected.** Four fixes, four detectors, four green tests.

**What happened.** All four tests passed. All four bugs were still
there when I played it.

**Why.** Two layers of wrong. The detectors tested convenient
stand-ins rather than the thing I had complained about — the score
detector sampled one sprite slot out of five, and it happened to be
the one that gets repositioned on every frame regardless, so it always
looked stable. Underneath that, they read the console's sprite memory
through an emulator call that returns `0xFF` whenever the display
hardware has that memory locked, which it does for part of every
frame. So the detectors had quietly defined "this element is visible"
as "we got `0xFF` back" — and `0xFF` came back reliably.

**What I changed.** Every detector now has to be *seen failing* on the
current build before the fix that makes it pass is allowed to land. If
it cannot be made to fail, it is not testing the bug. I also added
emulator commands that read sprite and palette memory directly,
bypassing the lock, so the measurement itself is not lying.

**What I would tell someone else.** Two separate things can be wrong
with a passing test: it may be checking the wrong property, or the
thing it reads may not be telling the truth. Both were wrong here. Make
a test fail on purpose before you believe it when it passes — and when
you are reading hardware, find out what your read returns while the
hardware is busy.

### 3. The rule that asked me to remember

The Game Boy addresses memory in 16KB banks, and code has to be
assigned to one. The project's instructions said, plainly and for
months: *run `romusage` after adding code, to check bank sizes.*

**What I expected.** Adding a feature is routine; the check happens
afterwards.

**What happened.** A bank filled up. The linker does not treat that as
an error — it keeps assigning addresses past the end of the bank. One
function ended up at address `0x837C`, which is not program memory at
all; it is inside video RAM. Anything calling it jumped into the
middle of the screen data and executed whatever the display happened
to be holding.

**What I saw.** "The emulator hard-resets." That is all. A jump into
garbage tends to land back at the cartridge entry point, which is
indistinguishable from a reset, and the emulator itself never crashed,
so there was nothing in any log.

**Why the rule failed.** It asked for a manual check *after* an
action. That is only as reliable as remembering, every time, forever —
and nobody adding forty lines to a menu handler thinks *this might be
the change that overflows a bank.*

**What I changed.** [`scripts/check_banks.py`](../scripts/check_banks.py)
reads the linker's own output and fails the build if any symbol or
section lands outside its bank. It runs on every link and cannot be
skipped. Separately, the ROM now writes a breadcrumb to
battery-backed save RAM as it moves through the game, so if it does
reset unexpectedly, the next boot can say where it was.

**What I would tell someone else.** Like people, agents are not 100% reliable. A rule that depends on an agent remembering is a rule with a failure rate. Where you can, convert it 
into something the build does for you. That gate has since caught the
same class of overflow twice more, in changes that were otherwise
completely routine.

### 4. The art that never reached the ROM

Puzzle levels are stored as data tables. I ran a pass to vary the
colors across them so the later puzzles did not all look alike.

**The rule at the time.** *Both test suites must pass before declaring
a change complete.* It was followed. Both were green, honestly.

**What happened.** The game kept showing the old colors.

**Why.** Two independent faults, each harmless alone. The level data
had at some point been split by hand into two files so it would fit in
separate banks — and the ROM compiles the split copies, while the unit
tests compile the original. The recolor pass edited the original. So
the tests were reading the new data and the game was reading the old
data, and each was perfectly self-consistent. On top of that, the
build had no dependency recorded between those data files and the
objects built from them, so even correcting the data would not have
triggered a rebuild.

**How it was found.** By searching the built ROM byte by byte for the
new color values and not finding them. A person going looking, not a
check.

**What I changed.** The split is now generated mechanically from the
original instead of maintained by hand; the build has real
dependencies; and a test compares the split files against their source
and fails if they diverge. That test was confirmed to fail on the
broken tree before the fix landed.

**What I would tell someone else.** A green suite tells you the code
*the tests compile* is correct. It says nothing about the code *the
product compiles*, once those two have drifted apart. Every time you
split, copy, or pre-process a source file you create a second copy the
tests may not be looking at — and a generated file somebody once edited
by hand has quietly stopped being generated.

### 5. The fix that did not travel

This repository was extracted from the parent project on August 16.

**What happened.** In the extracted version, the cursor froze during
match animations and then jumped to its new position when the
animation finished. In the parent project it moved smoothly.

**Why.** The parent project had fixed exactly this on August 17 — the
day *after* the extraction — by adding a hook that the animation code
calls once per frame so the game can keep drawing. The extracted copy
had the hook defined and the animation code calling it, but nothing
ever installed a function into it. The cursor's position was being
recalculated every frame and drawn by nobody.

**What the test suite said.** Nothing. Everything was green. The
extracted code and the parent code did not disagree about any value;
one of them simply had a wire nobody had connected.

**How it was found.** By playing the ROM.

**What I would tell someone else.** Comparing two implementations only
finds the places where they disagree. A feature that is missing from
both, or connected in neither, makes them agree perfectly. Testing by
comparison needs something beside it that is not a comparison —
somebody actually using the thing.

### 6. Twenty green tests and a letter one pixel too tall

The title screen uses a 16-pixel display font drawn specifically for
it. The test suite compares captured frames against approved reference
images.

**What happened.** The letter *A* was one row taller than the other
thirty-five characters, so the word on the title screen sat slightly
wrong in a way that is obvious once you see it. Later, the letter *I*
had a stem one pixel thinner than every other upright. Both shipped.

**Why the tests missed it.** Image comparison passes at a match
threshold. One row of one letter, in a font that occupies a small part
of the screen, does not move that number far enough to fail.

**How it was found.** By looking at the screen, twice, weeks apart.

**What I changed.** The font is now checked against its own
metrics — every letter must share one cap height, every upright must
carry the same stem weight — at build time, from the font data rather
than from a screenshot. That check fails on the old font.

**What I would tell someone else.** Compare screenshots when you want
to know whether something broke loudly. When the defect is small and
systematic — one row, one letter, every single time — a percentage
threshold absorbs it. If you can state the property exactly, check the
property and not the picture.

### 7. The editor deleted a symbol the linker needed

The tile and palette editor in `tools/sprite-editor/` reads and writes
the game's C source directly: its document *is* `res/tiles_data.c` and
`res/palettes.c`, so there is no export step to forget.

**What I expected.** Opening the editor on the example's assets and
saving without changing anything would be a no-op.

**What happened.** The ROM stopped linking.

**Why.** `res/palettes.c` held two arrays: the background palettes,
which the editor knows about, and the sprite palettes, which it does
not. On save, the editor wrote the file back out with the one array it
understood, and the other simply ceased to exist. From the editor's
point of view it had done its job perfectly.

**What I changed.** The sprite palettes moved into a file the editor
does not own. And a test now fails if any symbol the editor cannot
round-trip is found living in a file the editor rewrites, so the next
version of this mistake is caught at build time rather than at link
time.

**What I would tell someone else.** Ownership of a file has to be
all-or-nothing. If a tool rewrites a file, it must be able to
reproduce everything in it — otherwise the parts it does not
understand are on a timer.

---

## Delegation

### 8. The color fitter lost to a person

The Game Boy Color allows eight background palettes of four colors
each, on screen at once. Fitting a piece of artwork into that is
fiddly and I had been doing it by hand in the editor. In August I
tested whether a current model could automate it.

**What happened.** It produced something genuinely clever: a technique
that streams new palettes into the hardware between scanlines, so more
than the nominal eight are visible in a single frame. More color on
screen than the constraint appears to allow.

**And.** The output still did not look as good as the versions I had
tuned by hand. Not close enough to switch.

**What I changed.** The character portraits went back to the
hand-tuned data. The technique did not get thrown away — it became a
mode inside the editor, with per-band palette controls, so I drive it
rather than receive its output.

**What I would tell someone else.** When automation loses on taste,
that is usually an argument for putting it inside the instrument
rather than for deleting it. And taste is not a capability problem:
when a model can eventually do this well, its taste still will not be
mine.

---

## What paid off

### 9. The oracle found three bugs in the original

Before replacing the game's engine, I built a reference
implementation of it in Python — ported from the original C, deliberately
reproducing its quirks — and locked the two together by running ten
thousand generated boards through both and comparing every
intermediate state. The last build of the original game was archived
as a ROM and kept as a permanent point of comparison.

**What I expected.** Confirmation that the new engine matched the old
one.

**What happened.** It did. It also surfaced three real defects in the
*original* game, which had been shipping: a signed-arithmetic mistake
that could place a tile outside the board, an input swallowed on the
first frame after entering a mode, and a bonus condition that checked
the wrong pass of a cascade.

**What I would tell someone else.** An equivalence oracle is pointed
at both implementations, not only the new one. If you build one to
make a rewrite safe, expect it to tell you things about the code you
were trying to preserve.

### 10. The slicing was what made it safe

Replacing the engine of a working game is the kind of task that is hard
to hand to anybody, human or otherwise. The result is difficult to read,
easy to get subtly wrong, and wrong in ways that only show up in play
weeks later.

**What I did instead of reviewing it.** Eleven numbered steps over five
days, August 11 to 15. The first five built the measuring equipment: a
Python client that drives the emulator, a headless build of it that runs
without a window, savestates that jump straight to any point in the
game, a scenario suite, and a determinism proof — ten thousand generated
boards producing the same result on every run.

Only then did the replacement start. Each of the next six steps ended in
a claim I could check:

- the Python reference implementation produces identical results to the
  original engine across ten thousand boards
- the new C engine produces identical results to the Python reference
- the new ROM boots and plays
- each game mode in turn behaves identically to the original
- the whole ROM matches the archived original, byte for byte, in
  endless, timed and battle

**What the switch actually was.** Deleting nine files — the main loop,
the renderer, and the screen code for every mode — and building the ROM
from the new runtime instead. The title screen came out pixel-identical
to the last build of the original. The old ROM stayed in the repository
as a permanent point of comparison, so the equivalence claim is still
checkable today rather than being a thing I asserted once.

**What I would tell someone else.** You do not have to review the
change. You can review the claim it makes about itself, as long as the
claim is small enough to be plainly true or plainly false. "This engine
is correct" is not something I can check. "These ten thousand boards
produce the same result in both versions" is — I run it, and it either
holds or it does not. I never read the engine rewrite. I read eleven
statements like that one, and the size of the change stopped being the
thing that limited me.
