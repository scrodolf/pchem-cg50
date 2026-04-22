# PChem - Physical Chemistry Add-in for CASIO fx-CG50

A six-topic educational add-in covering quantum mechanics, spectroscopy,
and many-electron atoms. Built on the modern **fxSDK + gint** stack with
a custom 2D math rendering engine.

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
