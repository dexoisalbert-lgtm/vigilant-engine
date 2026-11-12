# Paleotrench — Asset List (3D models and sound effects)

Derived from [GDD.md](GDD.md). The greybox build needs none of these; this is the shopping/production list for the art swap (M14) and audio pass (M12).

**Priority:** **P1** needed for the first boss cave to feel real (M7) · **P2** needed for the full game · **P3** polish / nice to have.
**Type:** **R** = rigged and animated · **S** = static mesh · **M** = modular kit piece (reused many times) · **F** = needs fur/cloth/soft-body or special shader.

---

# PART 1 — 3D MODELS

## 1.1 Bosses (4 hero assets — spend the biggest share of the art budget here)

| # | Asset | Type | Pri | Notes |
|---|---|---|---|---|
| B1 | Livyatan melvillei (full body, ~17 m) | R | P1 | Huge teeth, melon organ weak point as separate mesh, scars and barnacles variant textures |
| B2 | Livyatan — damaged states (3: melon wounded, jaw wounded, enraged) | S/R | P1 | Texture/blendshape layers, not separate rigs |
| B3 | Livyatan — jaw interior (for phase-3 grab sequence) | S | P1 | Tongue, palate, teeth ring, close-up detail |
| B4 | Tusoteuthis longa (mantle, fins, head, beak, ~10 m) | R | P2 | Camouflage shader-ready (chromatophore mask) |
| B5 | Tusoteuthis — 8 arms + 2 hooked feeding tentacles (separate, severable) | R | P2 | Hook rows, sucker detail, cut-stump mesh |
| B6 | Tusoteuthis — beak and eye close-up (phase 3) | S | P2 | |
| B7 | Stygiomedusa gigantea (bell ~1 m, 4 oral arms ~10 m) | R/F | P2 | Cloth/soft-body arms, bioluminescent veins, vent-burn damage state |
| B8 | Stygiomedusa — arm segments (damage and severed variants) | R | P2 | |
| B9 | Stygiomedusa — bell interior (phase-3 overload) | S | P2 | |
| B10 | Arthropleura (~2.5 m, ~30 leg pairs) | R | P2 | Plate segments individually breakable, soft-joint weak points visible |
| B11 | Arthropleura — damaged states (cracked plates, severed legs) | S/R | P2 | |
| B12 | Arthropleura — moulted shell/exoskeleton (scannable, 3 weak-point variants) | S | P2 | Placed in caves for the exposure objective |

## 1.2 Fauna (hostile)

| # | Asset | Type | Pri | Notes |
|---|---|---|---|---|
| F1 | Voltaic eel (fictional electric eel, ~3 m) | R | P1 | Glowing electric-organ band, crevice-ambush pose, 2 size variants |
| F2 | Electric organ (harvested, carryable) | S | P1 | Wet, gory, glowing; 3 decay stages (fresh / dim / rotten) |
| F3 | Jaekelopterus (giant sea scorpion, ~2.5 m) | R | P1 | Claws, tail, silt-burst ambush |
| F4 | Helicoprion (tooth-whorl shark, ~7 m) | R | P2 | Tooth-whorl is the hero feature; pack of 3 variants |
| F5 | Xiphactinus (fanged fish, ~5 m) | R | P2 | |
| F6 | Dunkleosteus (armoured placoderm, ~6 m, mini-boss) | R | P2 | Plate armour + shear-blade jaw |
| F7 | Rhizodus (giant lobe-fin) | R | P3 | |
| F8 | Juvenile mosasaur (mini-boss, ~8 m) | R | P2 | |

## 1.3 Fauna (territorial / neutral / harmless)

| # | Asset | Type | Pri | Notes |
|---|---|---|---|---|
| F9 | Cameroceras (orthocone nautiloid, ~5 m shell) | R | P2 | |
| F10 | Anomalocaris (~1 m) | R | P2 | |
| F11 | Archelon (giant sea turtle) | R | P3 | |
| F12 | Stethacanthus (shark with anvil fin) | R | P3 | |
| F13 | Ammonite (3 sizes/shell patterns) | R | P2 | Simple swim loop, instanced in numbers |
| F14 | Trilobite (3 species) | R | P2 | Crawl + curl-up when startled |
| F15 | Belemnite shoal unit (single animal, instanced as shoal) | R | P2 | |
| F16 | Crinoid ("sea lily") meadow pieces (3 variants) | S/M | P2 | Sway shader |
| F17 | Hallucigenia | R | P3 | |
| F18 | Tullimonstrum ("Tully monster") | R | P3 | |
| F19 | Bioluminescent plankton / jelly (small drifting units, 3 variants) | R | P2 | Light-emitting |
| F20 | Generic small fish shoal unit (2 species) | R | P2 | False-scare fodder |

## 1.4 Corpses and remains (gore)

| # | Asset | Type | Pri | Notes |
|---|---|---|---|---|
| C1 | Dead-creature carcass base (large, medium, small) with dismember and bite-wound variants | S | P1 | Reused with different skins; attracts scavengers |
| C2 | Whale-fall skeleton (Livyatan cave centrepiece) | M | P1 | Ribs, vertebrae, skull; walkable-through |
| C3 | Previous diver remains (3 poses, partial suits) | S | P1 | Story logs placed with them |
| C4 | Fossil/skeleton fragments (ribs, skulls, tusks) | M | P2 | Cave dressing |
| C5 | Severed-limb/organ chunks (eel, arm, tentacle) | S | P2 | Drift physics |
| C6 | Player death gore pieces (for first-person death cams, per boss) | S | P2 | Tied to death animations |

## 1.5 Player and gear (first person: hands and tools are seen constantly, high detail)

| # | Asset | Type | Pri | Notes |
|---|---|---|---|---|
| P1 | First-person arms and gloves (suit sleeves, wrist computer mount) | R | P1 | Full animation set listed in section 3 |
| P2 | Dive mask frame and visor overlay (edge LED, scratches, droplets mesh) | S | P1 | |
| P3 | Head-mounted flashlight (5 brightness levels: Off/Low/Med/High/Burst) | S | P1 | Dial/indicator states |
| P4 | Wrist dive computer (screen mesh, strap, button states) | R | P1 | Raise animation |
| P5 | Battery pack (back-mounted, hose ports, charge LEDs) | S | P1 | Plug-in animation with charge cable |
| P6 | Charging cable and connectors (flashlight, crossbow, radio, scanner) | S | P1 | |
| P7 | Air tank (main) + regulator + hoses | S | P1 | 3 upgrade tiers |
| P8 | Spare air tank (pickup and swap versions) | S | P2 | |
| P9 | Pony bottle (emergency air) | S | P3 | |
| P10 | Fins (visible at screen bottom while kicking) | S | P2 | Silent-fin upgrade variant |
| P11 | Dive suit body (third-person for corpses/cutscenes; 3 upgrade looks) | R | P2 | |
| P12 | Waterproof radio handset/clip-on | S | P1 | |
| P13 | Hand-crank charger (pack fallback) | S | P2 | |
| P14 | Med kit | S | P2 | |
| P15 | Scanner attachment (codex scanning) | S | P2 | |

## 1.6 Weapons and tools

| # | Asset | Type | Pri | Notes |
|---|---|---|---|---|
| W1 | **Volt Crossbow** (5 charge-segment lights, charge port, 3 upgrade looks) | R | P1 | The most important prop in the game |
| W2 | Crossbow bolt (standard) | S | P1 | |
| W3 | Bolt variants: barbed, explosive tip, electric-tip, tranquiliser, armour-piercing | S | P2 | 5 meshes |
| W4 | Dive knife | R | P1 | |
| W5 | Hand spear (2 tip upgrades) | R | P2 | Thrown and retrieved |
| W6 | Speargun/harpoon gun + tether reel | R | P2 | |
| W7 | Harpoon shaft (stuck-in-target and stuck-in-rock versions) | S | P2 | |
| W8 | Bang stick / powerhead | R | P3 | |
| W9 | Flare (lit and unlit) + flare stick | S | P2 | |
| W10 | Noise lure (thrown device) | S | P2 | |
| W11 | Mining drill (3 upgrade tiers) | R | P1 | |
| W12 | Sonic disruptor (Livyatan exposure device) | S | P2 | |
| W13 | UV light attachment | S | P2 | |

## 1.7 Pickups and resources

| # | Asset | Type | Pri | Notes |
|---|---|---|---|---|
| R1 | Mineral nodes: pyrite, quartz, magnetite, amber, chronolite (rare) — intact + mined states | S | P1 | 5 × 2 = 10 meshes |
| R2 | Mineral chunks (carryable, one per type) | S | P1 | |
| R3 | **Core Shards** (4, one per boss, unique look each) | S | P1 | Glow + hum |
| R4 | **Minor shards** (3 shapes) | S | P2 | |
| R5 | Resonator parts ×3 (Livyatan exposure) | S | P1 | |
| R6 | Thermal-vent catalysts ×3 (Stygiomedusa exposure) | S | P2 | |
| R7 | Tusoteuthis nest sites ×3 variants (destroyable) | S | P2 | |
| R8 | Ammo cache / supply crate | S | P1 | |
| R9 | Log/journal items (diver notebooks, voice recorder, dive slate) | S | P2 | 4 types |
| R10 | Fauna toxin gland (tranquiliser source) | S | P3 | |

## 1.8 Cave modular kit (procedural generator uses these; keep Nanite/mobile LOD in mind)

| # | Asset | Type | Pri | Notes |
|---|---|---|---|---|
| E1 | Rock wall/floor/ceiling tile sets, one per biome (5 biomes × ~6 pieces) | M | P1 | Livyatan: pale limestone; Tusoteuthis: dark basalt; Stygiomedusa: black volcanic glass; Arthropleura: wet sandstone/coal; hub: mixed strata |
| E2 | Stalactite / stalagmite / column sets (small, medium, large) | M | P1 | Breakable versions for the Livyatan arena |
| E3 | Rubble, boulders, scree piles | M | P1 | Arthropleura-cave footstep surface |
| E4 | Tunnel squeeze, shaft mouth, ledge pieces | M | P1 | |
| E5 | Rock arches and bridges | M | P2 | |
| E6 | Cave-in blockage (intact + collapsed) | M | P2 | Livyatan rams |
| E7 | Hydrothermal vent chimney (smoker, active + dormant) | M | P2 | Stygiomedusa cave, boss lure target |
| E8 | Air-pocket dome kit (ceiling, waterline ring, ledge, dry floor) | M | P1 | Checkpoint location |
| E9 | Hiding spots: crevice, overhang, narrow gap | M | P1 | Tusoteuthis hiding game |
| E10 | Ink-gallery vertical shaft pieces | M | P2 | |
| E11 | Water surface/seam meshes (air-pocket waterline) | S | P1 | |
| E12 | Silt and sand beds | M | P2 | |
| E13 | Coal-seam / fossil-wall decal meshes | M | P3 | |

## 1.9 Flora and marine decoration

| # | Asset | Type | Pri | Notes |
|---|---|---|---|---|
| V1 | Prehistoric kelp-like forest (3 species, 3 sizes each) | M | P1 | Sway shader, hiding cover |
| V2 | Giant fern/horsetail/lycopod stumps (Carboniferous, Arthropleura cave) | M | P2 | |
| V3 | Coral/sponge/stromatoporoid clusters | M | P2 | |
| V4 | Tube worm / vent-life clusters | M | P2 | |
| V5 | Bioluminescent fungi and anemones | M | P2 | Light sources for Black Veil |
| V6 | Oxygen-bubbling plant/vent (decorative) | M | P3 | |
| V7 | Seaweed strands, algae mats, sea grass | M | P2 | |

## 1.10 Hub: The Confluence

| # | Asset | Type | Pri | Notes |
|---|---|---|---|---|
| H1 | Wrecked time machine (damaged + 4 repair stages + fully repaired) | S/R | P1 | 4 shard sockets, final activation animation |
| H2 | Workbench / upgrade station | S | P1 | |
| H3 | Air dome structure and entry hatch | S | P1 | |
| H4 | Four cave-mouth portals (one per boss, biome-themed) | S | P1 | |
| H5 | Storage lockers/crates | S | P2 | |
| H6 | Earlier-expedition camp props (tents, tables, lamps, tools) | M | P2 | |
| H7 | Time-strata cavern walls (mixed-era rock layers) | M | P2 | |
| H8 | Hub breach-event damage states | M | P3 | Late-game attack |

## 1.11 Boss arena set pieces

| # | Asset | Type | Pri | Notes |
|---|---|---|---|---|
| A1 | Livyatan arena: cathedral vault, breakable stalactite ring | M | P1 | |
| A2 | Tusoteuthis arena: ink chamber, feeding pillars | M | P2 | |
| A3 | Stygiomedusa arena: vent field, bioluminescent plateau | M | P2 | |
| A4 | Arthropleura arena: floodable chamber, flood gates and controls | M | P2 | Animated gates |
| A5 | Eel nest (one per arena, 3 variants) | S | P1 | Eel spawn points |

## 1.12 Prologue: boat and rift

| # | Asset | Type | Pri | Notes |
|---|---|---|---|---|
| O1 | Research vessel (exterior + deck, with dive platform) | S | P2 | |
| O2 | Captain's cabin / radio room (interior) | M | P2 | Radio as hero prop |
| O3 | Dive gear rack and prep props | S | P2 | |
| O4 | Modern reef (corals, fish, rocks) for the calm opening dive | M | P2 | |
| O5 | Trench wall with rift site | M | P2 | |
| O6 | Wormhole / rift volume mesh (for shader) | S | P2 | |
| O7 | Captain character model (seen in flashback/ending) | R | P3 | Only if a visual appearance is used |

## 1.13 Environment gameplay props

| # | Asset | Type | Pri | Notes |
|---|---|---|---|---|
| G1 | Checkpoint/air-pocket marker (portable marker light) | S | P1 | |
| G2 | Flood gate/valve control panels | S | P2 | |
| G3 | Ancient artefacts / ruined structures (rift-era debris) | M | P3 | |
| G4 | Shipwreck/debris from other eras (hulls, anchors, cannon) | M | P3 | |
| G5 | Mine-cart style rails, crates, scaffolding (previous expedition) | M | P3 | |
| G6 | Ink-cloud / blood-cloud / silt volume proxy meshes | S | P3 | Niagara supports |

## 1.14 UI-facing 3D (in-world)

| # | Asset | Type | Pri | Notes |
|---|---|---|---|---|
| U1 | Codex scan hologram frame | S | P3 | |
| U2 | Map ping/marker meshes | S | P3 | |

## 1.15 VFX meshes and textures (not full models, but need authoring)

Niagara sprites and meshes: marine snow, silt puff, bubble stream, bubble burst, blood cloud, ink cloud, electric arc (several shapes), spark burst, flare glow/smoke, drill debris, explosion bubble, impact splash on flesh/rock/armour, bite-spray, breath-bubbles, mask-edge droplets, caustics light function, rift shader, vent plume, bioluminescent pulse.

## 1.16 Model totals (rough)

| Category | Approx. count |
|---|---|
| Bosses (incl. states/parts) | 12 |
| Fauna (all species) | ~20 |
| Gore/remains | ~6 families, ~25 variants |
| Player/gear/weapons | ~30 |
| Pickups/resources | ~25 |
| Cave kit (modular) | ~70 pieces |
| Flora/decor | ~30 |
| Hub, arenas, boat | ~35 |
| **Total** | **~250 distinct meshes, of which ~35 are rigged** |

---

# PART 2 — SOUND EFFECTS

Convention: each line is a *family*; "×N" is the number of variations to generate so repetition isn't audible. Add 2–3 randomised pitch layers in MetaSounds. Underwater versions are mixed with a low-pass in-engine; record/generate them "dry" unless marked **(wet)**.

## 2.1 Player body and movement

| Sound | Variations | Pri |
|---|---|---|
| Regulator inhale / exhale (calm, ×3 intensity levels) | 3 × 4 | P1 |
| Breathing: panicked, exhausted, low-oxygen gasp | 3 × 3 | P1 |
| Held breath / suppressed breathing (stealth) | ×3 | P2 |
| Exhale bubble bursts (loop + one-shot) | ×4 | P1 |
| Heartbeat loop at 4 intensities | 4 loops | P1 |
| Fin kick (normal, silent-upgrade, sprint) | 3 × 4 | P1 |
| Body drag on rock / suit scrape | ×6 | P1 |
| Water movement swish (slow/medium/fast) | 3 × 3 | P2 |
| Entering/leaving water at air pockets | ×4 | P1 |
| Footsteps in air pockets on: wet rock, gravel, sand, shell, metal | 5 × 6 | P1 |
| Crawl / crouch-walk (Arthropleura cave) | ×6 | P2 |
| Gear rattle, tank clank on rock | ×4 | P2 |
| Pain grunts (light, heavy, bleeding), muffled by mask | ×10 | P1 |
| Blackout / oxygen-out sequence (gasp, silence, tinnitus) | ×2 | P1 |
| Death sounds per type (drown, bitten, electrocuted, crushed, envelop/dissolve) | 5 × 2 | P2 |
| Wound bleeding drips (muted) | ×3 | P3 |
| Swallowing/struggle/choke when enveloped | ×4 | P2 |

## 2.2 Gear and UI

| Sound | Variations | Pri |
|---|---|---|
| Flashlight click on / off | ×3 | P1 |
| Flashlight level change (5 levels) | 5 | P1 |
| Flashlight burst/strobe charge-up and fire | ×2 | P1 |
| Flashlight flicker / low-battery stutter | ×4 | P1 |
| Flashlight failure and dead | ×2 | P1 |
| UV light on / off | ×2 | P2 |
| Wrist computer raise / lower | ×2 | P1 |
| Wrist computer beeps: confirm, warning, critical, new-map-node, scan-complete | 5 | P1 |
| Oxygen low alarm (3 stages) + critical siren | 4 | P1 |
| Battery low / empty chirps | ×3 | P1 |
| Scanner sweep (loop) + success + fail | 3 | P2 |
| Map ping | ×2 | P3 |
| Tank swap (unclip, plug, hiss, re-seat) | ×3 | P2 |
| Pickup sounds per category: mineral, shard, ammo, log, med kit, organ | 6 × 2 | P1 |
| Inventory full / cannot-carry | ×2 | P2 |
| Menu: navigate, select, back, error, pause, resume | 6 | P2 |
| Save / checkpoint-activated chime | ×2 | P1 |
| Med kit use (spray + wrap) | ×2 | P2 |
| Rewarded-ad UI cues (mobile only) | — | P3 |

## 2.3 Electricity and charging

| Sound | Variations | Pri |
|---|---|---|
| Cable plug-in click and unplug | ×3 | P1 |
| Charging hum (loop, rising pitch for 5 crossbow levels) | 5 loops | P1 |
| Crossbow level-up chime (levels 1–4) + FULL charge (level 5) | 5 | P1 |
| Charge interrupted / discharge fizz | ×3 | P1 |
| Battery pack idle hum (low/high charge) | 2 loops | P2 |
| Hand-crank (loop) | ×2 | P2 |
| Electric arc crackle (small/medium/large) | 3 × 3 | P1 |
| Player electrocuted (shock zap, device scramble static) | ×4 | P1 |
| Capacitor charge-hold hum | ×1 loop | P3 |
| Overload discharge (Stygiomedusa phase 3 finale) | ×2 | P2 |

## 2.4 Weapons

| Sound | Variations | Pri |
|---|---|---|
| Volt Crossbow: draw/arm, aim hum, fire at each level 1–5 (5 distinct, level 5 massive) | 8 | P1 |
| Crossbow bolt flight whoosh, hit flesh/armour/rock/shell/metal | 6 | P1 |
| Crossbow dry-fire (not charged) | ×2 | P1 |
| Bolt pull-out (retrieval) from rock/flesh | ×4 | P1 |
| Dive knife swing, stab flesh, hit armour, slice (eel organ harvest) | 4 × 3 | P1 |
| Organ harvest: wet tear, squelch, cut | ×6 | P1 |
| Spear thrust, throw, hit, retrieve | 4 × 3 | P2 |
| Speargun fire, reel spool (loop), tether snap | 3 × 2 | P2 |
| Bang stick detonation (underwater blast) | ×3 | P3 |
| Barbed / explosive / electric / tranquiliser impacts | 4 | P2 |
| Flare ignite, burn loop, extinguish | 3 | P2 |
| Noise lure activate / ping loop | ×3 | P2 |
| Sonic disruptor charge / pulse | ×3 | P2 |

## 2.5 Mining

| Sound | Variations | Pri |
|---|---|---|
| Drill start-up, loop (rock hardness 3 tiers), stall, wind-down | 4 × 3 | P1 |
| Drill quiet-upgrade variants | ×2 | P2 |
| Mineral break/crumble, pickup clink (per mineral type) | 5 × 2 | P1 |
| Rock vibration rumble (drilled) | ×2 loops | P2 |
| Rock tick/creak (cave stress) | ×8 | P1 |
| Falling rock small/medium/large, cave-in | 4 | P1 |
| Stalactite shatter | ×4 | P1 |

## 2.6 Cave ambience (all loops, 60–120 s, seamless; stereo/ambisonic where possible)

| Sound | Pri |
|---|---|
| Deep-water drone base (per biome × 5) | P1 |
| Open cavern reverb bed (large, medium, small, tunnel) | P1 |
| Dripping/air-pocket ambience (dry, with water drips + room tone) | P1 |
| Hub ambience (safe, distant machinery, hum) + breach variant | P2 |
| Vent rumble / hydrothermal hiss / bubbling | P2 |
| Whale-fall creaks and distant groans (Livyatan cave) | P1 |
| Dense kelp creak and rustle (Tusoteuthis cave) | P2 |
| Black Veil: near-silence + soft bioluminescent pulse tones | P2 |
| Arthropleura tunnels: dripping, dry rattle, distant leg clatter | P2 |
| Marine snow/silt soft texture bed | P3 |
| Surface/boat ambience (waves, engine idle, gulls, ropes, deck) | P2 |
| Modern reef ambience (clicks, snapping shrimp, gentle bubbles) | P2 |
| Rift: ripping/pulling roar, reality-tear (cinematic) | P2 |
| Distant random one-shots: rock fall, far creature call, metal groan, bubble gurgle | ×20 | P1 |

## 2.7 Boss sounds

**Livyatan** (large, blind echolocator)
- Echolocation click trains: slow patrol, fast sweep, close-range lock (3 × 4) — these are a core gameplay cue, P1
- Click echoes returning from rock (wet, ×4)
- Idle bass vocalisations and distant call (×6)
- Roar/bellow (×3), enraged scream (×3)
- Ram impact on cave wall, cave-in follow-up (×4)
- Jaw snap, bite, thrash, tail-slap (×4 each)
- Sonar jammed: confusion bleats and thrashing (×4)
- Hurt/grunt and death moan (×3 each), whale-fall resonance on death
- Breath exhale through blowhole in air pockets (×2)

**Tusoteuthis** (deaf, visual hunter)
- Skin/chromatophore flutter (soft, ×3) — mostly silent, P2
- Arm grab, slide across rock, hook scrape (×6)
- Beak click and crack (×4)
- Water jet propulsion (small burst, big burst, ×4)
- Ink release whoosh and cloud (×3)
- Feeding tentacle whip and strike (×4)
- Arm severed: wet snap/tear (×3), pain jet and thrash (×3)
- Hurt, enraged, death gurgle (×3 each)

**Stygiomedusa** (no ears, no eyes; drawn to light and electricity)
- Veil rustle/drift (cloth-like, 3 speeds, ×3)
- Arms brushing rock, curtain ripple (×4)
- Reaction hum when lit (rising, ×3)
- Digestion/envelop sloshing, sizzle on contact (×4)
- Vent-burn hiss and shriek (×3)
- Bell pulse thrum (rhythmic loop, ×3)
- Electricity attraction crackle, overload shatter (×3)
- Death: collapse and deflate (×2)

**Arthropleura** (poor sight/hearing, vibration-sensitive)
- Leg clatter (walking, running, ceiling crawl, ×6 each), the main warning cue, P1 for that cave
- Chitin plate scrape and creak (×4)
- Mandible click and hiss (×4)
- Rearing, slam attack, charge (×3 each)
- Armour crack, joint pierce, severed leg (×4)
- Wet/dry air-pocket entry sounds, plunge into water (×3)
- Hurt screech, enraged screech, death rattle (×3 each)

**Common to all bosses**
- Arena entrance rumble/roar, stinger music cues for each phase transition
- Weak-point-expose sounds (glowing/flesh-opening) ×3
- Enrage buildup, death sequence, Core Shard release (crystal ring-out)

## 2.8 Creature sounds (per species; each ~6–10 variations unless noted)

| Species | Sounds needed |
|---|---|
| Voltaic eel | Idle crackle, hiss, strike shock, charge-up warning (key audio cue), death fizzle, eel-nest chatter |
| Jaekelopterus | Claw clacks, water-burst ambush, silt scuttle, chitin hiss, death |
| Helicoprion | Fast water flow, tooth-whorl crunch, pack calls, death |
| Xiphactinus | Splash charge, jaw snap, thrash |
| Dunkleosteus | Armour plate clank, jaw shear (big crunch), roar-grunt |
| Mosasaur (juvenile) | Deep growl, bite, tail whip |
| Rhizodus, Stethacanthus, Archelon, Cameroceras | Simple movement + low call sets |
| Anomalocaris | Appendage flicker, crunch |
| Ammonites / trilobites / belemnites | Small scuttle, shell tick, jet pulse (tiny, many) |
| Hallucigenia / Tullimonstrum | Soft squelch, tiny clicks |
| Bioluminescent plankton | Gentle chime/sparkle texture |
| Shoal scatter | Panic burst whoosh (an early-warning cue for boss approach) |
| Generic: attack-telegraph, hit-reaction, death for each size class (small/medium/large) | ×6 each |

## 2.9 Radio, voice and story

| Sound | Pri |
|---|---|
| Radio click on / off / squelch, press-to-talk | P1 |
| Static bursts (light, medium, heavy, near-boss) | ×8, P1 |
| Radio distortion/warble stretch (time-slip), reversed-speech texture | ×6, P1 |
| Half-word glitch cutouts (generated from recorded lines) | P2 |
| Dive computer synth voice ("Oxygen low") — optional | P3 |
| Captain dialogue (~80–120 lines, English; subtitles + placeholder voice first) | P2 |
| Captain breathing/mic noise, boat intercom | P2 |
| Previous-diver audio logs (~10–15 recordings, with degradation) | P2 |
| Rift sequence audio: voice stretch, pitch-bend, silence | P2 |
| Ending narration/voice (4 endings) | P3 |

## 2.10 Environment, props and interactions

| Sound | Pri |
|---|---|
| Time machine: idle hum, shard-insert (×4), repair stages (×4), power-up, activation, collapse | P1 |
| Workbench: craft, upgrade success, upgrade fail, mineral deposit | P2 |
| Air-dome hatch open/close, pressurise hiss, dome ambient | P2 |
| Flood gate valve turn, water rush, drain gurgle (Arthropleura arena) | P2 |
| Vent eruption, steam burst, thermal catalyst hiss | P2 |
| Cave collapse sequence (ends tunnels, finale) | P2 |
| Finale escape rumble, rapid-ascent, water tear | P2 |
| Bubble streams (loop), bubble pop, large bubble burst | ×6, P1 |
| Debris impacts (metal, wood, bone, shell) | ×8 each, P2 |
| Thrown object hits (lure, flare, rock) | ×4 each, P2 |
| Boat: engine, wake, rope, ladder, dive entry splash, rain | P2 |
| Surfacing / ascending breath-out | ×3, P2 |

## 2.11 Horror/scare sounds

| Sound | Pri |
|---|---|
| Stingers: short (1–2 s), medium (3–5 s), long (6–10 s), ×8 each, dissonant, wet, metallic and organic variants | P1 |
| "Something's there" low swells for scare-director near-miss events | ×10, P1 |
| Eerie vocal/whale-like droning textures | ×6, P2 |
| Light-flicker electrical stutter with silence cut | ×4, P1 |
| Distant roars heard through rock (muffled) for 4 bosses | ×3 each, P1 |
| Fake-out/false-scare sounds: harmless fauna passing, bubbles, a drifting corpse thump | ×10, P2 |
| Silence-drop effects (audio duck + tinnitus) | ×3, P1 |
| Tension riser loops (3 intensities) + release thud | P1 |

## 2.12 Music (outside "sound effects", for planning)

| Piece | Pri |
|---|---|
| Menu theme | P2 |
| Hub theme (calm uncertainty) | P2 |
| 4 cave explore beds (very sparse; mostly drones) | P2 |
| 4 boss stalk loops (minimal) | P2 |
| 4 boss fight themes with 3 phase layers each | P2 |
| Boat prologue, rift, ending themes, credits | P2 |
| Low-oxygen panic layer | P2 |

## 2.13 Sound totals (rough)

| Category | Approx. files |
|---|---|
| Player body and gear/UI | ~160 |
| Electricity and weapons | ~110 |
| Mining and environment | ~110 |
| Ambience loops | ~35 |
| Bosses | ~230 |
| Other creatures | ~220 |
| Radio, voice, logs | ~40 sfx + ~110 voice lines |
| Horror stingers/scares | ~80 |
| Music cues | ~35 |
| **Total** | **~1,000 audio files** (plus mobile-compressed versions) |

---

# PART 3 — Animations needed (not models, but required for the rigged ones)

| Rig | Required animations |
|---|---|
| First-person arms | Idle (breath sway), swim kick forward/back/strafe/ascend/descend, sprint, flashlight dial, raise wrist, crossbow equip/aim/fire/recoil/charge-hold (levels 1–5), cable plug-in/out (each device), knife swing/stab/harvest, spear thrust/throw, drill hold/active, tank swap, med kit, pickup, push off rock, grab ledge, grabbed-struggle ×4, death cam ×6, air-pocket stand, crouch-walk |
| Bosses | Swim/crawl cycles (speed variants), turn, idle, attack set (3–5 per boss), telegraph, hit reaction (4 directions), stagger, enrage, death, phase transition, weak-point expose |
| Mid fauna | Swim, turn, idle, attack, telegraph, hit, death (procedural spine swim reduces hand-keyed work) |
| Small fauna | Single swim/crawl loop + startled reaction |
| Eels | Ambush strike, coil, swim, charge-up flare, shock, death, harvest-ready |
| Props | Time machine repair stages, flood gates, drill, crossbow charge segments |

---

# Sourcing notes

- **Stock/library first** for ambience, water, rock, debris, and generic UI (Fab, freesound CC0, commercial libraries). **Generate/record custom** for bosses, creatures, charging/crossbow, and electric eels — these define the game's identity.
- **Bosses and the Volt Crossbow first.** If time runs short, fauna and flora can be bought off-the-shelf; the four bosses and the crossbow cannot.
- Every asset goes into a data-asset slot (see GDD §11), so any placeholder can be swapped without code changes.
