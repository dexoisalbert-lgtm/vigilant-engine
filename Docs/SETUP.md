# Setting up your PC

One-time setup, about 1–2 hours, most of it download time. You need roughly
**150 GB free** on an SSD. Do this on the RTX 4060 laptop first; repeat on the
RTX 5090 laptop whenever you want to use it. **Both laptops must use the same
engine version.**

## 1. Install Visual Studio 2022 (the C++ compiler)

1. Download **Visual Studio 2022 Community** (free) from visualstudio.microsoft.com.
2. In the installer, tick these two workloads:
   - **Desktop development with C++**
   - **Game development with C++**
3. Under "Individual components", make sure a **Windows 11 SDK** is ticked.
4. Install. You never need to open Visual Studio itself for this project.

## 2. Install Unreal Engine 5.7

1. Install the **Epic Games Launcher** from unrealengine.com and sign in.
2. Go to **Unreal Engine → Library → Engine Versions → +**.
3. Pick version **5.7** (the newest 5.7.x) and install it to the default location.

The project is pinned to 5.7. Please don't use a different version without
telling me, because the code is written against it.

## 3. Install Git and Git LFS

1. Install **Git for Windows** from git-scm.com (default options are fine).
   Git LFS is included.
2. Open **Git Bash** once and run:
   ```
   git lfs install
   ```

Git LFS stores the large Unreal asset files. Without it, levels and models
arrive as tiny broken placeholder files.

## 4. Get the project

In Git Bash, in the folder where you want the project (keep the path short and
without spaces, e.g. `C:\Dev`):

```
git clone https://github.com/dexoisalbert-lgtm/vigilant-engine.git Paleotrench
cd Paleotrench
git checkout ccr-c50880a2-g0siyj
```

## 5. Build and open

1. Double-click **`Tools\Build.bat`**. The first build takes several minutes.
   Wait for `==== BUILD SUCCEEDED ====`.
2. Double-click **`Tools\PrepareM1.bat`** with the editor closed. Wait for
   `M1 PREPARATION SUCCEEDED`. This creates the test tank and imports tuning.
3. Double-click **`Paleotrench.uproject`**. The first launch compiles shaders
   and can take 10–20 minutes; later launches are fast.
4. Follow [Playtests/M1.md](Playtests/M1.md) to verify the build and swimming.

## Everyday loop

1. `git pull` in Git Bash (close the Unreal Editor first).
2. Double-click `Tools\Build.bat`.
3. If `Data/SwimTuning.csv` changed, rerun `Tools\PrepareM1.bat`.
4. Open `Paleotrench.uproject` and follow the current playtest checklist.
5. If anything fails or looks wrong: double-click `Tools\CollectLogs.bat` and
   paste the result into the chat (it is copied to your clipboard automatically).

## If something goes wrong

| What you see | What to do |
|---|---|
| `Unreal Engine 5.7 was not found` | Engine is not installed, or is in a custom folder. Finish step 2, or tell me the folder. |
| `BUILD FAILED` | Run `Tools\CollectLogs.bat` and paste the result. |
| Missing `M1_TestTank` map, or `M1 preparation failed` | Run `Build.bat`, then `PrepareM1.bat` with Unreal closed. If it still fails, collect logs. |
| "Missing modules… Would you like to rebuild them now?" when opening the project | Click **No**, run `Tools\Build.bat` instead (it gives readable errors). |
| "This project was made with a different version of the engine" | You installed a version other than 5.7. Install 5.7. |
| The build complains about a missing compiler/toolchain/SDK version | Paste the message; it names the exact Visual Studio component to add. |

## Later (not needed now)

- **Android builds**: Android Studio + SDK/NDK, set up when the mobile port starts.
- **iOS builds**: require a Mac. We'll decide how to handle that at the mobile stage.
