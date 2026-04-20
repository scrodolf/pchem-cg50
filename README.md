# PChem — Physical Chemistry Add-in for CASIO fx-CG50

A six-topic educational add-in covering quantum mechanics, spectroscopy, and
many-electron atoms.  Built on the modern **fxSDK + gint** stack with a
custom 2D math rendering engine modeled after Eigenmath.

---

## Architecture Overview

```
pchem-cg50/
├── CMakeLists.txt          # fxSDK CMake build configuration
├── include/
│   ├── pchem.h             # Screen geometry, color palette, state machine
│   ├── menu.h              # Scrolling menu data structures & API
│   ├── render.h            # Math AST (18 node types), font tiers, sym table
│   ├── topics.h            # Topic screen dispatch & content API
│   └── input.h             # Keyboard abstraction (getkey wrapper)
└── src/
    ├── main.c              # Entry point, state machine, event loop
    ├── menu.c              # Menu drawing, scrolling, selection handling
    ├── render.c            # Pool allocator, symbol table, layout+draw engine
    ├── topics.c            # Topic content with sample equations per topic
    └── input.c             # Key repeat config, getkey wrapper
```

### Rendering Engine (render.h / render.c)

The math rendering engine uses the **Eigenmath two-pass architecture**:

1. **Pass 1 — `render_layout()`** (bottom-up): Recursively computes
   `(width, height, baseline)` for every AST node, starting from leaves.
   Fractions stack numerator + bar + denominator. Rows align children by
   baseline. Scripts demote to FONT_SMALL tier.

2. **Pass 2 — `render_draw()`** (top-down): Recursively draws each node
   at its assigned `(x, y)` position, computing child positions from the
   parent's bounding box and the child's layout measurements.

**18 AST node types** via tagged union:
- Leaves: `TEXT`, `NUMBER`, `SYMBOL`
- Structural: `FRACTION`, `SUPERSCRIPT`, `SUBSCRIPT`, `SQRT`, `PAREN`, `ROW`
- Physics: `INTEGRAL`, `SUMMATION`, `BRA`, `KET`, `BRAKET`, `SANDWICH`, `HAT`, `BAR`, `ARROW`

**Memory**: Static pool of 256 nodes (~22 KB). No malloc. Reset per screen.

**Font tiers**: `FONT_LARGE` (24px), `FONT_NORMAL` (18px), `FONT_SMALL` (11px conceptual).

**Symbol table**: `sym("psi")` → `"\xE5\xB8"` (OS multi-byte ψ glyph).
42 pre-loaded symbols covering Greek letters and math operators.

### State Machine

```
STATE_MAIN_MENU ──[EXE]──► STATE_TOPIC_VIEW ──[EXIT]──► STATE_MAIN_MENU
       │                            │
       └──[MENU]──► EXIT APP ◄──[MENU]──┘
```

### Key Bindings

| Key  | Action                        |
|------|-------------------------------|
| ▲/▼  | Scroll / navigate             |
| EXE  | Select / enter                |
| EXIT | Go back one level             |
| MENU | Exit add-in (from any screen) |

---

## Implementation Guide: Step-by-Step

### Step 1: Install the fxSDK + gint Toolchain

The fxSDK by Lephenixnoir is the modern Linux-based build system for the
fx-CG50.  It provides:
- `sh-elf-gcc` cross-compiler (SuperH4 target)
- `gint` freestanding kernel (replaces the Casio OS during add-in execution)
- `fxsdk` CLI tool for project management and build

**On Debian/Ubuntu:**

```bash
# 1. Install dependencies
sudo apt install cmake make git python3 python3-pil libusb-1.0-0-dev \
     libsdl2-dev pkg-config gcc g++ flex bison

# 2. Clone and install the GCC cross-compiler for SuperH
#    (The fxSDK wiki provides an automated script)
git clone https://gitea.planet-casio.com/Lephenixnoir/GiteaPC.git
cd GiteaPC
python3 giteapc.py install fxsdk

# 3. This installs: sh-elf-gcc, fxsdk CLI, gint library, fxconv, fxg3a
#    The install process takes 20-40 minutes (compiles GCC from source).

# 4. Verify installation:
sh-elf-gcc --version        # Should show the SH cross-compiler
fxsdk --version             # Should show the fxSDK version
```

**On Arch Linux:**

```bash
# AUR packages are available:
yay -S fxsdk-git sh-elf-gcc-git gint-git
```

**On macOS:**  Follow the fxSDK wiki instructions for Homebrew-based install.

### Step 2: Create the fxSDK Project

```bash
# Option A: Initialize with fxsdk (creates boilerplate)
fxsdk new pchem-cg50
cd pchem-cg50

# Option B: Use our existing files directly
# Copy the pchem-cg50/ directory from this archive to your workspace.
# The CMakeLists.txt is already configured for fxSDK.
```

If using Option A, replace the auto-generated `CMakeLists.txt`, `src/`, and
`include/` with the files from this project.

### Step 3: Create Icon Assets

The `.g3a` binary requires two icon images.  Create placeholder icons:

```bash
mkdir -p assets

# Create 92x64 unselected icon (shown in the Casio MAIN MENU)
convert -size 92x64 xc:white -fill black -gravity center \
    -pointsize 14 -annotate +0+0 "PChem" assets/icon-uns.png

# Create 92x64 selected icon (highlighted state)
convert -size 92x64 xc:blue -fill white -gravity center \
    -pointsize 14 -annotate +0+0 "PChem" assets/icon-sel.png
```

(If you don't have ImageMagick, any 92×64 PNG will work.)

### Step 4: Build the .g3a Binary

```bash
cd pchem-cg50/

# Configure the build (first time only)
fxsdk build-cg configure

# Build the add-in
fxsdk build-cg

# The output is: build-cg/PChem.g3a
# This is the binary that runs on the calculator.
```

**What happens during the build:**
1. CMake configures the cross-compilation with `sh-elf-gcc`
2. Each `.c` file is compiled to a `.o` object file targeting the SH4 CPU
3. Objects are linked against `libgint` (the freestanding kernel)
4. `fxg3a` tool wraps the ELF binary into the `.g3a` Casio add-in format
5. Icon PNGs are converted and embedded into the `.g3a` header

### Step 5: Transfer to the Calculator

**Method A: USB Mass Storage (simplest)**

1. Connect the fx-CG50 to your computer via USB cable
2. On the calculator, press **F1** (USB Flash) when the connection dialog appears
3. The calculator mounts as a USB drive on your computer
4. Copy `build-cg/PChem.g3a` to the root of the USB drive
5. Safely eject the drive
6. On the calculator, press **EXIT** to disconnect

**Method B: fxlink (fxSDK's transfer tool)**

```bash
# Install fxlink (part of fxSDK)
# Connect calculator via USB, then:
fxlink -s build-cg/PChem.g3a
```

### Step 6: Run the Add-in on the Calculator

1. From the calculator's **MAIN MENU**, scroll to find the **PChem** icon
   (it appears in the add-in section alongside other .g3a apps)
2. Press **EXE** to launch
3. You should see the scrolling menu with six PChem topics
4. Use **▲/▼** to navigate, **EXE** to enter a topic
5. Inside a topic, **EXIT** returns to the menu, **MENU** exits the app

### Step 7: Troubleshooting

| Problem | Solution |
|---------|----------|
| Icon doesn't appear | Ensure .g3a is in the root directory, not a subfolder |
| "Invalid add-in" error | Rebuild with `fxsdk build-cg` — ensure icons are valid PNGs |
| Greek symbols render as boxes | Verify OS font byte codes; test with `dtext(10, 10, C_BLACK, "\xE5\xB8")` |
| Screen flickering | Ensure `dupdate()` is called exactly once per frame, after all drawing |
| Pool exhausted (NULL node) | Check `render_pool_used()` — may need to increase MATH_NODE_POOL_SIZE |
| Build fails: "gint not found" | Run `fxsdk build-cg configure` again; verify gint is installed |

---

## Verifying the Symbol Table

To test that OS font glyphs render correctly on your specific calculator
and OS version, add this temporary test in `main.c` before the event loop:

```c
/* Temporary symbol test — remove after verification */
dclear(C_WHITE);
dtext(10, 10, C_BLACK, "Greek: ");
dtext(80, 10, C_BLACK, "\xE5\xB8");  /* should show ψ */
dtext(100, 10, C_BLACK, "\xE5\xB9"); /* should show ω */
dtext(120, 10, C_BLACK, "\xE5\xA0"); /* should show α */
dtext(140, 10, C_BLACK, "\xE5\xA1"); /* should show β */
dtext(10, 30, C_BLACK, "Math: ");
dtext(80, 30, C_BLACK, "\xE5\x84");  /* should show Δ */
dtext(100, 30, C_BLACK, "\xE5\x93"); /* should show Σ */
dtext(120, 30, C_BLACK, "\xE5\xB0"); /* should show π */
dtext(140, 30, C_BLACK, "\xE5\xD0"); /* should show ∞ */
dupdate();
getkey();  /* Wait for keypress before continuing */
```

If any glyph renders incorrectly, adjust the corresponding byte pair in
the `sym_table_init()` function in `render.c`.

---

## Next Steps

- [ ] Populate each topic with full explanatory text and equation trees
- [ ] Add numerical input fields for interactive calculations
- [ ] Implement text wrapping for long descriptions
- [ ] Add custom bitmap font for true FONT_SMALL rendering
- [ ] Build a simple expression parser for user-entered formulas
