# PALEOTRENCH — Game Plan

## 1. Context

You want a first-person underwater horror game in Unreal Engine, built in this repo (`dexoisalbert-lgtm/vigilant-engine`), PC first (Steam, KB/M + gamepad), mobile later (free with ads/IAP). A scuba diver is pulled through a wormhole into a prehistoric cave system and must kill four bosses, collect shards, and repair a time machine before oxygen runs out.

Decisions locked from your answers:

| Topic | Decision |
|---|---|
| Title / project name | **Paleotrench** |
| Scope | Full game, 8+ hours, all four bosses at first PC release |
| Bosses | Tusoteuthis, Livyatan, Arthropleura, Stygiomedusa — all kept, lore adapted |
| Boss structure | Stalk phase through the cave, then kill in a final arena |
| World | Central hub + 4 cave branches, any order |
| Caves | Fully procedural, random; seed fixed per save file |
| Death | Selectable difficulty: checkpoints (Normal), loot-drop (Hard), permadeath (Nightmare) |
| Oxygen | Air pockets (free refill + checkpoint) and spare tanks; tank upgradeable |
| Weapons | Retrievable + craftable ammo, special ammo from rare minerals |
| Power | Kill eels, harvest organ → battery pack → manually charge devices |
| Boss damage | Only a fully charged (level 5 of 5) eel-powered Volt Crossbow hurts bosses; on Hard the pack can't feed both head flashlight and crossbow |
| Mining | Slow, noisy drilling that attracts creatures |
| Fauna | Rich ecosystem of real species |
| Story | Captain's voice in fragments through static; subtitles + placeholder voice first |
| Tone | Graphic, Mature |
| HUD | Diegetic wrist dive computer (minimal overlay added for mobile) |
| Extras | Creature codex, multiple endings, localisation, accessibility |
| Art | Greybox first, real art swapped in later |
| You | Unreal beginner; RTX 4060/16 GB laptop (daily), RTX 5090/64 GB laptop (occasional) |

### Constraints that shape everything
- **I cannot run the Unreal Editor or compile Unreal code from the cloud.** I write C++, config, data files, editor-automation Python and guides; you pull, build and playtest on your laptop and send back logs/screens. So the design is **C++-first and data-driven**, with almost nothing requiring hand-made Blueprints.
- **Engine-free logic is tested by me before you ever see it.** Cave generation, oxygen/battery maths, crafting, save data and AI sense maths live in plain C++ with no Unreal headers, with a test suite I run here.
- **8+ hours, solo, beginner, realistic creatures is a very large project.** Procedural caves and systemic AI supply the hours; the plan is staged so there is a playable build early and every milestone is shippable as-is.
- **"Realistic looking" is gated by art, not code.** Greybox gets the game fun and scary first; section 11 covers the art swap.
- **Mature gore + free-with-ads on mobile conflict**: ad networks and stores restrict graphic content. The mobile build ships with reduced gore by default and age gating (section 12).

---

## 2. Step 0 — Repo reset (needs your go-ahead when we start)

The repo currently contains a complete Roblox Luau plane game (7 commits, 41 files: `src/`, `docs/GAME_PLAN.md`, `rokit.toml`, etc.).
1. Tag the last Roblox commit `archive/roblox-plane-game` so it stays recoverable.
2. `git rm` everything; commit "Remove previous project".
3. Add Unreal `.gitignore`, `.gitattributes` with **Git LFS** for `*.uasset`, `*.umap`, audio, textures.
4. All work goes on branch `ccr-c50880a2-g0siyj`; no PR unless you ask.

---

## 3. Your machine setup (guide I'll write: `Docs/SETUP.md`)

- Epic Games Launcher → **latest stable Unreal Engine 5** (version pinned in `Paleotrench.uproject`; both laptops must match).
- Visual Studio 2022 with "Game development with C++" workload; Git + Git LFS.
- Later, for mobile: Android Studio/SDK/NDK via Unreal's `SetupAndroid` script. iOS builds need a Mac — flagged as a future decision.
- Daily loop: `git pull` → double-click `Tools/Build.bat` → open project → play → run `Tools/CollectLogs.bat` and send me the output if anything breaks.
- 4060 laptop = daily development and the **minimum-spec benchmark**. 5090 laptop = lighting builds, packaging, high-settings capture.

---

## 4. Technical architecture

### Repo layout
```
Paleotrench.uproject
Config/                  DefaultEngine/Game/Input/Scalability + Android/iOS device profiles
Source/
  PaleotrenchCore/       engine-free C++ (std only): cave graph, resource maths, crafting, save schema
  Paleotrench/           game module: player, gear, weapons, AI, bosses, world, UI, audio, save
  PaleotrenchEditor/     editor utilities: greybox asset bootstrap, data validation
Content/                 binary assets (LFS) — created by scripts or by you in-editor
Data/                    CSV/JSON source of truth for all tuning (imported to DataTables)
Tools/                   Build.bat, CollectLogs.bat, CoreTests (CMake), Python editor scripts
Docs/                    SETUP, GDD, per-milestone playtest checklists, art briefs
```

### Key technical choices
- **C++ for all gameplay; Blueprints only as thin data-only subclasses** generated by a Python editor script, so you never wire graphs by hand.
- **Everything tunable lives in `Data/*.csv`** (creature stats, weapon damage, oxygen rates, upgrade costs, spawn tables) so balance changes need no code.
- **Swimming**: custom 6-degrees-of-freedom movement mode on `UCharacterMovementComponent` — inertia, drag, buoyancy, slow turn while sprinting, fin-kick noise events. No simulated water surface except air pockets and the boat intro.
- **Underwater look**: exponential height fog + distance-based colour absorption post-process, Niagara marine snow and silt, caustics light function near openings, mask-edge vignette and droplets. Fully dynamic lighting (procedural levels cannot be baked): flashlight spotlight + emissive bioluminescence. Lumen is a High/Ultra option only; the default path is cheap enough for mobile.
- **AI**: State Trees per creature, `UAIPerceptionComponent` with custom senses (below), Environment Query System for hiding/ambush spots, 3D navigation via a sparse voxel graph generated with the cave (NavMesh does not work for swimming volume).
- **Unified stimulus system** — the heart of the horror. Every player action emits typed stimuli that creatures sense differently:
  - `Sound` (fin kicks, breathing bubbles, drill, weapon fire, radio)
  - `Light` (flashlight level, flares, bioluminescence)
  - `Vibration` (touching rock, mining, walking in air pockets)
  - `Electric` (charging devices, eel discharge)
  - `Blood` (wounds, corpses — spreads over time)
- **Input**: Enhanced Input with an abstract action layer (KB/M, gamepad now; touch later with no gameplay code changes).
- **Save**: versioned `USaveGame` holding seed, hub state, inventory, upgrades, boss flags, codex, checkpoints. Steam Cloud on PC.
- **UI**: UMG widgets driven from C++ (CommonUI for gamepad/touch focus). String Tables for every line of text from day one (localisation).
- **Audio**: MetaSounds; underwater low-pass, occlusion via cave graph, sound-class ducking for breath/heartbeat.
- **Scalability**: Low/Medium/High/Ultra tiers defined at the start; "Low" is the future mobile profile.

### Procedural cave generation (seed fixed per save)
1. **Graph** (engine-free, tested here): per-cave seed = save seed + cave ID → nodes (chambers, air pockets, mineral rooms, nests, set-piece rooms, boss arena) and edges (tunnels, squeezes, vertical shafts, loops for escape routes). Rules guarantee: solvable, air pocket spacing within oxygen range for the current tier, at least two routes around choke points, arena furthest from entrance.
2. **Geometry**: tunnels = noise-displaced tubes swept along splines; chambers = signed-distance blobs; meshed in chunks (dynamic mesh), collision generated async, streamed by distance.
3. **Set-piece templates**: hand-authored room templates (boss arenas, the hub, story rooms, special scares) slot into graph nodes via sockets. Authored in C++/data for greybox; replaced by art-dressed level instances later.
4. **Population**: spawn tables per biome for fauna, eels, mineral nodes, shards, logs, ammo, hiding spots (kelp-like growths, crevices, shipwreck debris from other eras).
5. **Scare director**: places scripted-feeling moments procedurally (silhouette passing a tunnel mouth, radio burst, light flicker, corpse drift) using pacing rules — tension builds, releases, never repeats twice in a row.
6. **Debug**: seed displayed in pause menu; `Tools/cavegen` CLI prints an ASCII/SVG map of any seed so I can reproduce your bug reports here.

---

## 5. Story and structure

- **Prologue — The boat.** Dawn, small research vessel. Radio check with Captain (tutorial for radio, wrist computer, flashlight charging). Routine trench dive: swimming, light levels, a calm modern reef for contrast.
- **The Rift.** At depth, a distortion in the trench wall pulls you through. Gear glitches, the captain's voice stretches into static.
- **The Confluence (hub).** A vast cavern where time strata collide — which is why creatures of different eras coexist (Carboniferous Arthropleura, Cretaceous Tusoteuthis, Miocene Livyatan, and Stygiomedusa as an ancient lineage that never died out). Here: a wrecked time machine left by an earlier expedition, a workbench, an air dome, storage, and four cave mouths.
- **Goal.** The machine needs **4 Core Shards** (one inside each boss's lair, released on its death) plus **minor shards** scattered through caves (each cave has more than required; extras feed endings and upgrades).
- **Radio.** The captain reaches you in fragments. Using the radio costs battery and makes sound. Time runs differently: his messages arrive out of order, and logs from earlier divers suggest he has sent people down before.
- **Endings** (driven by shards, logs found, and a final choice):
  1. *Return* — minimum repair; you surface, something came with you.
  2. *True Return* — all minor shards; clean jump, captain confronted.
  3. *Stay/Seal* — destroy the machine to close the rift.
  4. *Lost* — die during the final escape sequence on Nightmare.
- **Finale.** Activating the machine triggers a collapse and a timed escape swim with every surviving creature agitated.

---

## 6. Core gameplay systems

### Oxygen (the clock)
- Tank drains continuously; faster when sprinting, panicking (near bosses), or wounded. Low oxygen: tunnel vision, loud heartbeat, slowed movement, then blackout.
- Refill free at **air pockets** (also checkpoints, safe-ish rooms where you can stand) and at the hub. **Spare tanks**: found or crafted, swapped manually with an animation that leaves you vulnerable.
- Upgrades: capacity, rebreather efficiency, emergency pony bottle.

### Electricity
- **Battery pack** holds charge; **flashlight, radio, wrist computer scanner, electric ammo** each have their own small cell.
- Charging is manual: hold to plug the device into the pack (hands occupied, emits `Electric` stimulus, takes seconds).
- **Flashlight** brightness levels: Off / Low / Medium / High / Burst (strobe stun, heavy drain). Higher levels reveal more and attract more. Flicker when cell is low. With the light off, the scare director gets bolder and eyes/shapes appear at the edge of vision.
- **Eels** (a fictional prehistoric electric species, e.g. "voltaic eel", presented as an undiscovered lineage): hunt in crevices, shock on contact (damage + drains devices + scrambles wrist computer). Kill → harvest **electric organ** (short gory interaction) → refills pack. Organs decay if carried too long. A weak hand-crank fallback on the pack prevents soft-locks (slow, noisy).

### The Volt Crossbow — the only boss-killer (your addition)
- The crossbow is an **electric weapon charged from eel power**, with **5 charge levels** shown as five lit segments on the weapon itself and on the wrist computer.
- Charging is manual from the battery pack, one level at a time: hold to charge, hands occupied, slowed, emits `Electric` + `Sound` stimuli. Each level takes longer and costs more pack charge than the last. Being hit interrupts the level in progress.
- Damage by level: 1–2 kill small fauna, 3–4 kill/stagger large fauna and mini-bosses, **only level 5 (full charge) damages a boss**. Anything less only makes a boss flinch. A shot always empties the crossbow back to 0.
- **Every boss kill is therefore a loop**: expose the weak point → fire a full-charge bolt → survive while recharging to level 5 with the boss enraged → repeat. Each boss needs several full-charge hits (3 on Normal, more on Hard/Nightmare). All other weapons and tools exist to stun, distract, expose weak points and buy charging time.
- Eel organs are the fuel, so boss arenas contain eel nests: you must kill eels mid-fight to keep the pack fed.
- **Charge vs. light dilemma**: the pack has one output — it charges the head flashlight *or* the crossbow, never both at once.
  - Normal: pack capacity is enough to keep both topped up if you harvest sensibly; the cost is time.
  - **Hard**: one eel organ cannot fully charge both. You must choose — see, or kill. Fights have stretches in darkness by design.
  - Nightmare: as Hard, plus charge leaks slowly from both devices.
- Upgrades: faster charging, charge retention, a capacitor that holds one spare level, quieter charging. No upgrade removes the level-5 rule.
- The flashlight is **head-mounted** (mask light), so it stays on while both hands are busy charging or aiming.

### Other weapons (support — cannot damage bosses)
| Weapon | Role | Notes |
|---|---|---|
| Dive knife | Start; harvesting, last resort | Used for eel organs and escaping grabs |
| Hand spear | Reusable melee/thrust | Upgradable tip; can be thrown and retrieved |
| Speargun (harpoon) | Heavy single shot | Slow reload; tether upgrade lets you reel the shaft back or pin creatures |
| Bang stick / powerhead | Contact explosive | Rare, very loud |
| Flares | Light + distraction | Lure light-seeking creatures away |
| Noise lures | Thrown sound decoy | Key against echolocators |

- Projectiles have drag and drop; shafts stick in creatures/rock and must be **swum to and pulled out**.
- **Crafted ammo** at hub workbench from minerals: standard, barbed (bleed), explosive tip, electric (uses charge), tranquiliser toxin (from fauna glands).
- Hit locations and weak points per species; armour deflects.

### Mining and upgrades
- **Drill** nodes over several seconds: emits `Sound` + `Vibration`, kicks up silt (blinds you), costs oxygen time. Can be interrupted and resumed.
- Minerals by rarity and depth (e.g. pyrite, quartz, magnetite, amber, "chronolite" rare). Carry limit; on Hard you drop the dive's haul on death.
- **Upgrade trees** at workbench: Suit (armour, thermal, silent fins), Tank, Light (efficiency, beam, UV mode that reveals trails), Pack (capacity), Radio (range, sonar ping), Weapons, Drill (quieter, faster), Wrist computer (map, scanner, threat indicator).

### Wrist dive computer (diegetic HUD)
Raise wrist to see oxygen, depth, pack and cell charge, compass, auto-map of explored graph, objectives. Peripheral cues without raising it: breath sound, a single mask-edge LED for oxygen and battery. Scanner mode fills the **Codex**.

### Health and injuries
Wounds bleed (emits `Blood` stimulus that draws predators), limb injuries slow swimming or shake aim, treated with limited med kits at air pockets. Graphic first-person death sequences per creature, with a reduced-gore setting.

### Difficulty
Normal (checkpoint respawn), Hard (hub respawn, loot dropped at body, scarcer supplies), Nightmare (permadeath, no map threat indicator). Accessibility assists are separate from difficulty.

---

## 7. The four bosses

Each cave has: a biome, a stalk phase where the boss roams the generated cave and reacts systemically, **three "exposure" objectives** that weaken it or reveal its weak point, then an authored arena fight in three phases. Killing it releases a Core Shard.

**Common kill rule:** in every phase below, the described action (stun, lure, sever, burn, pin) only *opens the weak point*; the actual damage is one **level-5 Volt Crossbow bolt** into it. Then the boss enrages while you recharge from 0 to 5, harvesting arena eels for power. The per-boss sense rules make recharging uniquely dangerous in each fight: charging is noisy for Livyatan, visible sparks for Tusoteuthis, an electric beacon for Stygiomedusa, and keeps you standing still on rock for Arthropleura.

### Livyatan melvillei — "The Drowned Cathedral" (huge open flooded caverns, whale-fall graveyards)
- **Sense**: blind; echolocation. Periodic click trains sweep the cave — anything moving fast during a sweep is "seen". Hears sound acutely.
- **Player rule**: move slowly and silently, freeze during clicks, use noise lures. Flashlight is safe; drilling is deadly.
- **Stalk**: patrols main galleries (too big for tunnels — tunnels are your refuge, but it rams walls causing cave-ins).
- **Exposure**: recover three resonator parts from an old expedition to build a sonic disruptor.
- **Arena**: phase 1 jam its sonar and strike the melon organ when stunned; phase 2 it thrashes blind, bring down stalactites; phase 3 inside-the-jaws grab sequence, harpoon the wounded organ.

### Tusoteuthis longa — "The Ink Galleries" (tight vertical shafts, kelp-like forests, drifting ink clouds)
- **Sense**: deaf; exceptional eyesight, tracks motion and light. Camouflages against rock.
- **Player rule**: break line of sight, hide in growths and crevices, lights off or low, move when it looks away. Noise is free here — mine all you want.
- **Stalk**: ambusher; hooked arms reach into tunnels and feel around; ink clouds blind you.
- **Exposure**: destroy three camouflage "nest" sites and craft UV light mode to see through its camouflage.
- **Arena**: phase 1 sever feeding tentacles when they grab; phase 2 fight in ink with UV; phase 3 it drags you toward its beak — spear the eye/beak.

### Stygiomedusa gigantea — "The Black Veil" (pitch-dark abyssal chambers, bioluminescent fauna)
- **Sense**: no eyes or ears; drawn to light and electricity. Four 10 m oral arms billow through whole rooms like curtains.
- **Player rule**: flashlight discipline in the darkest cave of the game; charging devices is dangerous; use flares to pull the veil elsewhere.
- **Stalk**: slow, silent, everywhere — arms drift through tunnels; contact envelops and digests (oxygen drains fast, struggle to escape).
- **Exposure**: collect three thermal-vent catalysts to craft incendiary/electric overload ammo; learn it retreats from vent heat.
- **Arena**: phase 1 lure arms into vents to burn them; phase 2 lights-out — navigate by bioluminescence to reach the bell; phase 3 overload it with a full battery-pack discharge (leaving you powerless for the escape).

### Arthropleura — "The Breathing Tunnels" (half-flooded maze with long air-filled crawlways)
- **Lore**: an air-breather that hunts the air pockets — the one place you thought was safe. From this cave on, checkpoints feel different.
- **Sense**: poor sight and hearing; feels **vibration through rock** and tastes blood.
- **Player rule**: in air sections you walk/crawl — every footstep, dropped item and drill hit carries. Crouch-walk, avoid loose gravel, slip back into water (it cannot swim well, but it can hold its breath and follow briefly).
- **Stalk**: 2.5 m armoured body pours through crawlways and across ceilings; you hear its legs before you see it.
- **Exposure**: find three soft-joint weak points via codex scans of moulted shells; craft armour-piercing tips; flood controls for the arena.
- **Arena**: phase 1 hit underbelly when it rears; phase 2 flood/drain the chamber to control where it can go; phase 3 pin it with tethered harpoons and finish at the head.

---

## 8. Ecosystem (real species, data-driven)

All defined in `Data/Creatures.csv` using shared behaviour archetypes, so adding a species is mostly a data row plus a mesh.

- **Hostile**: Jaekelopterus and other eurypterids (sea scorpions, ambush from silt), Helicoprion (whorl-tooth, fast pack hunter), Xiphactinus (charging fish), Dunkleosteus (armoured mini-boss guarding rare minerals), Rhizodus, Mosasaur juvenile (open-water mini-boss), voltaic eels.
- **Territorial/neutral**: Orthocone nautiloids (Cameroceras), Anomalocaris, Archelon (giant turtle), Stethacanthus.
- **Harmless / atmosphere / false scares**: ammonites, trilobites, belemnite shoals, crinoid "meadows", Hallucigenia, Tullimonstrum, bioluminescent plankton.
- **Systemic behaviour**: predators hunt prey and each other, corpses attract scavengers, blood spreads, shoals scatter when a boss approaches (an early-warning system the player learns to read).
- **Codex**: scan to unlock entry with real facts (era, size, diet, fossil sites) + gameplay hints. Clearly labels the two fictional liberties (voltaic eel; where each animal really lived).

---

## 9. Horror direction

- **Sound first**: breathing regulator, heartbeat, muffled distance, creature vocalisations placed by cave graph. Radio static intensifies near bosses and bursts with half-words.
- **Darkness as mechanic**: light reveals and attracts; turning it off invites the scare director.
- **Scare director**: tracks a tension value; schedules ambient, near-miss and direct scares with cooldowns so procedural caves still feel authored.
- **Never fully safe**: air pockets are safe until the Arthropleura cave teaches otherwise; the hub is breached once late-game.
- **Graphic content**: per-creature death sequences, eel organ harvest, whale-fall carcasses, previous divers' remains. Reduced-gore toggle swaps blood for dark silt and cuts away from deaths.
- **Accessibility**: subtitle size/background, colour-blind palettes, camera shake/bob sliders, hold-vs-toggle, aim assist (gamepad), arthropod-phobia mode (reshapes Arthropleura and eurypterids), reduced-intensity mode for thalassophobia, audio cue visualiser.

---

## 10. Development roadmap

Each milestone ends with: my engine-free tests passing, a build you compile, a short playtest checklist in `Docs/Playtests/`, and a pushed commit.

| # | Milestone | What you can do at the end |
|---|---|---|
| M0 | Repo reset, project skeleton, setup guide, build/log scripts, CoreTests harness | Project opens and compiles on your laptop |
| M1 | Swimming, camera, input (KB/M + gamepad), underwater look, test tank level | Swim around a greybox tank; feels like diving |
| M2 | Oxygen, battery pack, flashlight levels, manual charging, wrist computer, save/load | Survive on a timer, manage light |
| M3 | Cave generator: graph → geometry → streaming, 3D nav, debug map CLI | Explore an endless variety of seeded caves |
| M4 | Stimulus system, creature AI archetypes, eels + 4 lesser species, health/injury | Caves are alive and hunting you |
| M5 | Weapons, Volt Crossbow 5-level charging, retrievable ammo, mining, minerals, workbench, crafting, upgrades | Full core loop |
| M6 | Hub, time machine, shards, checkpoints, difficulty modes, cave branch travel | **Vertical slice** structure: hub → cave → back |
| M7 | Boss 1 Livyatan (stalk AI, exposure objectives, arena) | First complete cave |
| M8 | Boss 2 Tusoteuthis | |
| M9 | Boss 3 Stygiomedusa | |
| M10 | Boss 4 Arthropleura + air-section movement | All four caves |
| M11 | Prologue boat, rift sequence, radio dialogue system, logs, endings, finale escape | Start-to-credits playable |
| M12 | Scare director, full ecosystem roster, codex, audio pass | Content complete (greybox) |
| M13 | Menus, settings, accessibility, localisation pipeline, gamepad polish | Feature complete |
| M14 | Art swap (section 11), lighting, VFX, performance pass on 4060 laptop | Beta |
| M15 | Steam: Steamworks, achievements, cloud saves, store page assets, demo, playtests, bug fixing | **PC release** |
| M16+ | Mobile port (section 12) | |

Order within M7–M10 can change; each boss is isolated behind a common `ABossBase` + per-boss State Tree and data.

---

## 11. Art and audio pipeline (greybox → final)

- Greybox uses primitive/capsule creatures with correct size, hitboxes and sockets, generated by `PaleotrenchEditor` so no manual asset work is needed to play.
- Every creature references assets through a **data asset slot** (mesh, anim blueprint, sounds, VFX). Swapping art = changing a row, no code.
- `Docs/ArtBriefs/` — one brief per creature: scale, paleontological references, required animations (swim, turn, attack set, hit, death, idle), bone/socket names, poly and texture budgets for PC and mobile LODs.
- Sources you can choose per asset later: Fab/Megascans (rock, coral, debris), commissioned or purchased rigged creatures, AI-assisted generation cleaned in Blender. Bosses deserve custom or high-quality purchased rigs; they sell the game.
- Procedural swim animation (spine-chain bending driven by velocity, Control Rig) reduces hand-animation needs for fish-like and tentacled creatures, and looks more natural in a random cave.
- Audio: placeholder library sounds → final pass; voice: subtitles + synthetic placeholder → actors for Captain and logs.

---

## 12. Mobile port plan (post-PC)

Prepared from day one: abstract input, scalability tiers, dynamic-only lighting, chunk streaming, data-driven asset slots with mobile LODs.

- **Rendering**: Mobile forward renderer, Vulkan (Android) / Metal (iOS), no Lumen/Nanite, cheaper fog and post-process, fewer dynamic lights (flashlight + 2–3), smaller streaming radius, lower creature counts via spawn-table multipliers, 30 fps target on mid-range devices.
- **Controls**: twin virtual sticks + ascend/descend buttons, gyro aim option, contextual interact button, aim assist, larger wrist-computer UI, optional minimal HUD overlay.
- **Sessions**: more frequent autosave at air pockets; suspend/resume safe.
- **Monetisation (free + ads/IAP)**, designed not to break horror: no banner ads in gameplay; optional rewarded ad for a checkpoint revive or a supply cache at the hub; one-time IAP removes ads; cosmetic gear skins; possibly first cave free with full-game unlock. No selling oxygen or power.
- **Store compliance**: reduced gore default, age gate, 17+/Mature rating, ad network content categories checked early; iOS requires a Mac for builds.
- **Size**: target under store cellular limits with on-demand asset packs per cave.

---

## 13. Risks

| Risk | Mitigation |
|---|---|
| I can't compile here; errors only show on your machine | Engine-free core tested here; small commits; `Build.bat` + `CollectLogs.bat` round-trip; strict use of stable, well-documented engine APIs |
| Scope (8+ h, 4 bosses, ecosystem) | Systems + procedural content provide hours; milestones M6 and M7 are natural early-access/demo points if needed |
| Random caves weaken authored horror | Scare director, authored set-piece templates, authored boss arenas |
| Procedural mesh + dynamic light performance | Chunked async generation, aggressive streaming, budget tested on the 4060 laptop every milestone |
| Realistic art for unusual animals is hard to source | Art briefs, procedural animation, bosses prioritised for budget |
| 16 GB RAM laptop with UE5 C++ builds | Unity builds tuned, small modules, heavy tasks on the 5090 laptop |
| Mature + ads on mobile | Reduced-gore mobile default, premium unlock path as fallback |

---

## 14. Verification

- **Here (every commit)**: `Tools/CoreTests` (CMake + g++) — cave graphs for thousands of seeds are connected, solvable and within oxygen range; resource, crafting and save round-trip tests; `cavegen` map output for visual inspection.
- **On your laptop (every milestone)**: `Tools/Build.bat` compiles; Unreal automation tests run from a script; you follow the milestone checklist in `Docs/Playtests/Mx.md` (specific things to try, expected results, what to screenshot) and send logs via `CollectLogs.bat`.
- **Performance**: `stat unit`/`stat gpu` captures on the 4060 laptop against per-tier frame budgets; seeds of any bad cave recorded for reproduction.
- **Before release**: full playthroughs on all three difficulties, all endings reached, gamepad-only run, accessibility options checked, localisation pseudo-language pass.

---

## 15. First actions once you approve

1. Step 0 repo reset (tag archive, wipe, LFS, ignore files).
2. M0: `Paleotrench.uproject`, three modules, configs, `Docs/SETUP.md`, build/log scripts, CoreTests harness, `Docs/GDD.md` (this plan, expanded as the living design document).
3. You install Unreal + Visual Studio following `SETUP.md` and confirm the empty project compiles — then M1 begins.
