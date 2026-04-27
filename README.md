# PChem - Physical Chemistry Add-in for CASIO fx-CG50

A six-topic educational add-in covering quantum mechanics, spectroscopy,
and many-electron atoms. Built on the modern **fxSDK + gint** stack with
a custom 2D math rendering engine.

## v6 updates

* **Greek glyph rendering -- root cause fix:** the fx-CG50 OS multi-byte
  page (\xE5xx) is **not** rendered by gint's default font, so the
  Greek byte sequences we previously emitted produced invisible glyphs
  on real hardware -- creating both the "missing Greek symbol" and the
  "unexpected gaps" complaints (the layout reserved 12 px for each
  glyph, which then drew zero visible pixels).  The pragmatic fix is
  to drop the OS multi-byte page entirely and use ASCII surrogates
  that gint's font definitely renders:

      psi  ->  y     mu    ->  u     beta    ->  b
      phi  ->  f     pi    ->  p     epsilon ->  e
      Delta->  D     lambda->  L     chi     ->  c

  (TeX-style mnemonics where possible.)  This trades the literal Greek
  glyph for guaranteed-visible output; equations now read
  `e^(-b*e_i)/q` instead of a blank-filled `e^(-?_i)/q`.  Once a true
  Greek bitmap font is integrated this can be reverted by simply
  swapping the symbol-table entries back to their OS bytes.

* **Subscripted Greek tokens:** the prose rewriter now matches Greek
  prefixes followed by `_` (subscript marker), so identifiers like
  `phi_1s`, `psi_n`, `epsilon_i`, `Delta_E` are rewritten to
  `f_1s`, `y_n`, `e_i`, `D_E` respectively.  Whole-word matches are
  preserved.  Non-Greek words that merely begin with a Greek letter
  (e.g. "chiasm", "phishing") are not rewritten -- the matcher
  requires a `_` boundary or end-of-word.

* **Width-allocation bug fixed:** with the OS multi-byte page gone,
  every glyph is now exactly one ASCII char wide.  The
  `tier_multibyte_w()` helper is no longer needed and the
  word-wrap layout no longer over-reserves space.  The unwanted gaps
  visible in IMG_8305 / IMG_8306 / IMG_8311 / IMG_8312 are eliminated
  because measured width and rendered width finally match.

## v5 updates

* **Dynamic equation wrapping with continuation markers:** very long
  equations (the molecular and Helium Hamiltonians, in particular) are
  now broken at logical operators (=, +, -) into multiple visible lines.
  Each non-final line ends with `...` and each non-first line begins with
  `...`, so the user can visually trace the break.  When splitting is
  impossible (a single fraction wider than the screen, or a single chunk
  still too wide after split) we fall back to FONT_SMALL demotion.
* **Greek text -> symbol substitution:** prose strings throughout the
  app now substitute whole-word Greek transliterations (psi, phi, Phi,
  Delta, alpha, beta, mu, omega, ...) with their actual OS-font glyphs
  at draw time.  Mixed identifiers like `psi_n`, `mu*r^2`, or "pi-system"
  are intentionally left untouched.
* **Delta rendering bug fixed:** `measure_text_width()` now reports the
  correct (wider) pixel width for OS multi-byte glyphs at each font
  tier, so capital Delta and other wide symbols no longer get clipped
  by adjacent characters in equation rendering.
* **Menu navigation v5:**
  - UP/DOWN: single-step on every press.  Holding for longer than half
    a second jumps to the top (UP) or bottom (DOWN) and locks until the
    key is released.
  - LEFT/RIGHT: strict single-step, no auto-repeat -- moving exactly one
    item per press regardless of how long the key is held.  RIGHT at
    the very last item is a no-op.
  Same rules apply to the per-topic submenu, content scrollers, and the
  global Navigation screen for UX consistency.

## v4 updates

* **Statistical Mechanics tab (Lecture 13):** new 7th topic covering
  microstates, permutations / combinations, weight W = N!/Pi a_j!,
  microstate probability P_i, Boltzmann distribution, partition
  function q, beta = 1/(kT), R = k N_A, expectation value, two-level
  populations.
* **Greek-letter prose rendering:** all six topic description blocks now
  embed actual OS-font Greek glyphs (psi, phi, Delta, alpha, ...) via
  byte-literal macros declared in pchem.h, instead of ASCII names.
* **Cut-off equation fix:** equations whose laid-out width exceeds the
  available content area are now auto-demoted to FONT_SMALL so the
  Helium and molecular Hamiltonians stay visible end-to-end on the
  396-pixel screen.

## v3 updates

* **HW 9-14 content:** 1s orbital + radial probability + most probable
  radius, Slater determinant, multi-electron / multi-nuclei
  Hamiltonian, overlap integral S_12, c_g and c_u coefficients,
  sp hybrid orbital construction, normalization / orthogonality /
  equal-contribution constraints, Boltzmann population ratio.

## v2 updates

* **Hierarchical menu:** every topic now leads to a submenu offering
  **Descriptions / Equations / Keywords**, so content is organised and
  browsable instead of one long scroll.
* **Per-equation variable descriptions:** each rendered equation is
  labelled and accompanied by a block describing its variables.
* **Global Navigation screen:** a seventh main-menu entry, `Navigation`,
  opens a single scrollable page that lists every keyword and every
  OS-font symbol used across all topics, divided by topic.
* **Full Lecture 8 + 9 + Many-Electron content** integrated into topics
  3, 4, and 5.
* **Paginated scrolling:** UP/DOWN scrolls one line at a time,
  F1/F2 pages through long content.

## Navigation model

```
MAIN_MENU --(topic EXE)--> SUBMENU --(subtopic EXE)--> TOPIC_VIEW
    |                         |                           |
    |                         +---[EXIT]------------------+
    |                                              <- back to SUBMENU
    +--(Navigation EXE)--> NAVIGATION --[EXIT]--> MAIN_MENU

    MENU: quit add-in (from anywhere)
    EXIT: back one level
```

## File layout

```
pchem-cg50/
  CMakeLists.txt
  include/
    pchem.h          Screen constants, AppState, TopicID, SubtopicID
    menu.h           Generic scrolling menu
    topics.h         TopicContent, EquationEntry, KeywordEntry,
                     SubMenuScreen, TopicScreen
    navigation.h     NavigationScreen API
    render.h         Math AST + symbol table API
    input.h          Keyboard abstraction
  src/
    main.c           State machine driver
    menu.c           Generic scrolling menu impl
    topics.c         Content data + submenu + content-view impl
    navigation.c     Global keyword/symbol navigator
    render.c         Math rendering engine
    input.c          Keyboard
  assets/            icon-uns.png, icon-sel.png (optional)
```

## Content modules

* **Topic 1 - Particle in a Box:** energy levels, wavefunction, Boltzmann
  population ratio (for HOMO/LUMO estimates); quantization, zero-point
  energy, nodes, tunneling, band theory, HOMO/LUMO, conjugation length.
* **Topic 2 - Commutators & Spin:** commutator definition, Heisenberg
  uncertainty, angular momentum, spin-1/2, Stern-Gerlach.
* **Topic 3 - Harmonic Osc. & Rotor:** oscillator energy, rigid rotor
  energy, Hermite polynomials, moment of inertia, zero-point vibration.
* **Topic 4 - Diatomic Spectroscopy (Lecture 8):** reduced mass, moment
  of inertia, rotational constant, wavenumber, selection rules,
  R-branch / P-branch, dynamic dipole.
* **Topic 5 - Hydrogen Atom (Lecture 9):** total Hamiltonian, Coulomb
  potential, Bohr radius, 1s orbital wavefunction, radial probability
  distribution, most probable radius, spherical harmonics, Laguerre
  polynomials, quantum numbers.
* **Topic 6 - Many-Electron Atoms (Lectures 10-14):** Helium Hamiltonian,
  separable wavefunction, effective one-electron Hamiltonian, trial
  wavefunction, variation theorem, SCF / Hartree-Fock, Slater
  determinant (with spin-orbitals and 1/sqrt(N!) prefactor), molecular
  Hamiltonian (multi-electron + multi-nucleus), overlap integral S_12,
  bonding/antibonding coefficients c_g = 1/sqrt(2(1 + S_12)) and
  c_u = 1/sqrt(2(1 - S_12)), sp hybrid orbital construction,
  normalisation/orthogonality/equal-contribution constraints.

## Build

```bash
fxsdk build-cg      # produces build-cg/PChem.g3a
```

Transfer the `.g3a` to the calculator via USB Flash (F1 dialog) and run
it from the MAIN MENU.

## Key bindings

| Key  | Action |
|------|--------|
| UP/DOWN | Navigate / scroll line by line |
| F1/F2   | Page up / page down in content views |
| EXE     | Open selection |
| EXIT    | Back one level |
| MENU    | Exit the add-in |
