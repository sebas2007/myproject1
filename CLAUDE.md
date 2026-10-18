# Project: Apollo 11 Pad — Bodycam Realism

2026-10-18 continuation: Docs/GEMINI1_PROTOTYPE_20261018.md records new mission
menu and isolated Gemini1 pad/White Room/interior/erector work. Startup now goes
to LVL_MissionMenu, with Apollo retaining its established predawn scene. M returns
to selection. Gemini-only daytime changes are explicitly authorized. Do not touch
Apollo cockpit, materials or lighting. Original exterior checklist still pending;
see Docs/EXTERIOR_CONTINUATION_20261018.md. Do not infer final verification from
written scripts. Native builds and map saves require the visible editor closed.

## Vision
Latest handoff (2026-10-18): Docs/REALISM_REVIEW_20261018.md and the last section of Docs/OVERNIGHT_LOG.md.
USER DIRECTION: tower work is additive. Preserve existing materials/appearance/layout. Material retuning from this pass was
rejected and the three changed MI assets restored byte-for-byte from M0 backup; defaults restored. Do not reapply it.
The intended gameplay role is an engineer/closeout helper assisting astronaut boarding. Ground-crew tasks need source support;
cabinet inspection is not a simulation of propellant servicing. No wrapping ramp without evidence and user approval.
Capsule view now uses a clean 85 degree helmet profile; outdoor bodycam is restored on exit. Fresh rendered picks/input/seats
PASS; result Saved/Playtest/realism_M1_result.json. build_weathered.build('surface_normals') is retired (no-op).
The original four-phase feature list remains incomplete; do not infer completion from scripts or older screenshots.
Additive tower handoff: Docs/TOWER_ADDITIONS_20261018.md. build_tower_fabrication.py adds connections/safety infill and
four E-operable inspection cabinets. User approved relocating only nine obstructing lower-floor hose reels; that repair is saved.
lower_catwalks job visits nine existing floors through 200; movement PASS, final visual clearance 0 blockers (3,876 samples).
tower:on tests all four cabinets; final tower_M4_render_result.json PASS, 1080p 100-116 FPS. M4_ screenshots include paired
before/after captures with the same existing materials. Engineer/helper boarding mode remains a direction, not a completed feature.

A hyper-realistic first-person experience set at Launch Complex 39A, Kennedy Space Center,
on the morning of the Apollo 11 launch (July 16, 1969). The visual target is the UE5 game
"Bodycam": footage that is hard to tell apart from real chest-mounted body-camera video.
No combat. The player is a pad closeout crew technician.

## Milestone 1 (current scope)
- Walk the pad around the Mobile Launcher.
- Climb the Launch Umbilical Tower (elevator + stairs) to the crew access arm level.
- Walk the crew access arm to the White Room and look through the open Command Module hatch
  into the cockpit. The player cannot enter the capsule yet.

## Historical accuracy rules
- Everything at true real-world scale (Unreal units = cm). Verify every dimension against
  NASA / primary sources before using it and note the source in a comment or doc.
- Key items to verify: Saturn V height and stage diameters; Mobile Launcher deck size and
  height; Launch Umbilical Tower height, level spacing and colors; crew access arm (Swing
  Arm 9) height, length and White Room layout; hammerhead crane; pad 39A flame trench and
  ramp; sun position and sky for pre-dawn July 16, 1969 at 28.6084 N, 80.6043 W.
- 1969-era hardware only: no modern equipment, signage, or lighting.
- Preferred references: NASA 3D Resources, Smithsonian 3D scan of Apollo 11 CM "Columbia"
  (interior), NASA photo archives, Apollo Lunar Surface Journal / Apollo Flight Journal.

## Build approach
1. Blockout: simple gray geometry at exact scale, fully walkable. Validate scale and flow.
2. Bodycam camera: first-person character with chest-height camera, inertia, head bob,
   handheld jitter, hands visible on railings/ladders.
3. Realism pass: replace blockout with detailed models (NASA / Fab / Sketchfab /
   Smithsonian), photoscanned materials (rusted steel grating, weathered red paint,
   concrete, frost on the rocket skin), Lumen GI, Nanite.

## Bodycam camera stack (post-process + camera)
- Very wide fisheye FOV with barrel distortion, chromatic aberration at edges.
- Sensor noise/grain, compression artifacts, mild over-sharpening, lens dirt and flares.
- Slow auto-exposure: blown highlights, crushed shadows.
- Rolling-shutter wobble and motion blur on fast turns.
- Timestamp + unit ID overlay in body-camera style (e.g. "1969-07-16 05:42:17").

## Atmosphere
- Pre-dawn: xenon searchlights on the white Saturn V, dark blue sky, faint horizon glow.
- Cryogenic vapor venting and drifting; wind gets stronger with height.
- Audio: muffled body-mic, wind buffeting, footsteps on steel grating, breathing,
  groaning metal, venting hiss, PA announcements echoing off the tower.

## Working rules for Claude
- Use the Unreal MCP tools for all editor work (server pinned to 0.5.30 to match the plugin;
  tools are separate, e.g. `manage_asset`, `control_actor`, not a single `unreal` tool).
  Save the level after each major step.
- set_viewport_camera in plugin 0.5.30 doesn't apply position/rotation — position the camera
  with Python instead (unreal.UnrealEditorSubsystem.set_level_viewport_camera_info), and
  verify with get_level_viewport_camera_info before taking a screenshot.
- Blockout is generated by scripts in Tools/Blockout/: build_pad39a_blockout.py first, then
  build_tower_climb.py. Re-run the scripts instead of hand-editing their actors.
  playtest_climb.py walks the route in PIE (start("elevator") / start("stairs"), then status()).
- Lit level LVL_Pad39A_Predawn is built by Tools/Lighting/build_predawn.py (stages: materials, sky,
  lights, stars, ground, post; vapor only when called explicitly). The visible twilight sky is drawn by the
  star dome (stars stage), not the SkyAtmosphere; the sky light is a captured scene, recaptured by the stars stage.
  Tuning values live in Docs/dimensions.md ("Predawn level lighting values"). Fog froxel cvars are in
  Config/DefaultEngine.ini [SystemSettings]. It refuses to run on any other level; it uses its own materials under
  /Game/Apollo11/Materials/Predawn so the blockout level is never changed.
- To make a copy of a level, open it and use EditorLoadingAndSavingUtils.save_map(world, newPath),
  then restart the editor before opening the copy. duplicate_asset + load_level in one session
  crashed the editor (world memory leak fatal error).
- MCP Niagara authoring (manage_effect add_emitter_to_system / add_*_module) reports success but
  produces systems with no emitters in this setup. Don't rely on it.
- Realistic Saturn V + Columbia CM in the predawn level: Tools/Realism/make_paint_maps.py (needs Pillow;
  writes SourceAssets/Generated) then Tools/Realism/build_saturn_realism.py (stages textures, rocket,
  material, cm, truss, place; build("restore_blockout") shows the hidden blockout rocket again).
- Import FBX/glTF with the pipelines in /Game/Apollo11/Import (static + combined; *_Nanite for scans).
  Interchange imports do not always save: save the assets afterwards.
- Don't wrap editor Python in contextlib.redirect_stdout (crashed the editor). Keep long jobs short or
  store their result in builtins and read it back; MCP calls time out after ~30 s but the job continues.
- Only one editor instance on this project at a time: a second instance locks the level files (saves fail).
- In materials, ObjectPositionWS is the bounds center; use ActorPositionWS for the actor origin. A texture
  sample's default output is RGB only; connect to its "RGBA" output to read alpha.
- The elevator is C++ (Source/MyProject1/Apollo11/BlockoutElevator). After changing C++, build the
  editor target with the editor closed; Live Coding can't be relied on for new classes/properties.
- Realism scripts added 2026-10-17 (all re-runnable, all on LVL_Pad39A_Predawn): Tools/Realism/build_tower_realism.py
  (tower bracing, piping, cable trays, open-truss swing arms 1-8 + umbilicals, hammerhead crane), build_whiteroom.py
  (White Room shell/lights/props/placards; make_placards.py makes the textures), build_night_fx.py (M_Vapor_Night +
  CryoVapor actor, PP_Bodycam + T_LensDirt), build_saturn_realism.build("night_paint") (panel lines + frost bands),
  build_predawn.build_mi() (material instances with the current weathering values). Blockout pieces they replace are
  hidden but keep their collision; the new meshes have no collision.
- New assets from that night live in /Game/Apollo11/{Materials,Meshes,Textures}/Night (the older Predawn/Realism
  assets were file-locked by a hung editor at the time). Night MIs are the ones in use.
- C++: ACryoVaporField (Apollo11/CryoVaporField: CPU vapor puffs as instanced quads, AS-506 vent layout) and
  UBodycamComponent + ABodycamHUD (Apollo11/BodycamComponent; on the character, on by default, F8 or console
  "Bodycam [0|1]"). Tuning values are UPROPERTYs; see Docs/dimensions.md.
- Headless work without the editor UI: Tools/run_cmdlet.ps1 -Script <py> -Call "<expr>" (Python commandlet, null RHI,
  loads the map, runs, saves). Play-test without rendering: Tools/Blockout/run_playtest_game.ps1 -Jobs
  "elevator,stairs" -ExtraArgs "-nullrhi" (results in Saved/Playtest/result.json). Screenshots + frame rates in a
  standalone offscreen game: Tools/run_morning_capture.ps1 (needs a working GPU).
- PIE and any sustained rendering hang (GPU timeout, GPU idle) while the Windows session is locked; that hang also
  left editor processes that could not be killed and kept project files locked until reboot. Don't start PIE
  unattended; use the null-RHI game for play-tests.
- Python 5.8: unreal.CustomInput() takes no constructor args; create it empty, then set_editor_property("input_name", ..).
- MaterialEditingLibrary pins: a node's single unnamed main input (Clamp, Transform, ...) is "" in 5.8, not "Input". A wrong
  pin name silently leaves it unconnected and the whole material (and every MI on it) falls back to the default material
  (2026-10-17: M_Real/M_Real_Grating broke this way and the pad stair, elevator and White Room rendered as gray blocks).
  After building a material, check the log for "Failed to compile Material".
- Verified screenshots: use the play-test game, Tools/Blockout/run_playtest_game.ps1 -Jobs "bodycam_on,shots:on,fps,bodycam_off,shots:off"
  (Saved/Playtest/night_P7_*.png, 9 views incl. two east-facing horizon views; job "pitch" checks bodycam vertical look).
  The editor viewport (4-pane layout) doesn't reliably keep a scripted camera, and AutomationLibrary.take_high_res_screenshot
  writes nothing in a -game process (the script uses the HighResShot console command instead).
- run_playtest_game.ps1 refuses rendering runs while the Windows session is locked (LogonUI running); only -nullrhi runs then.
- Stages added 2026-10-17: build_tower_realism.build("launcher") (ML base wall framing, deck handrail, 4 hold-down arms,
  3 tail service masts + hoses) and build_predawn.build("fog") (height-fog settings only). The ground stage uses the Night
  MIs when they exist (mi_in_use).
- Bodycam tunables: ExposureBias and MaxLookPitch are UPROPERTYs; vertical look is 1:1 with the input, clamped to +-85.
  UObjects used by the HUD must be held in UPROPERTYs (a function-static UFont* got garbage collected and crashed the game).
- VRAM: the Columbia scan's 8K textures are capped at 4096 (max_texture_size); keep new scan textures at <= 4K.
- If UBT says "Unable to build while Live Coding is active" but no editor is usable, add -NoHotReloadFromIDE.
- Play-through fixes 2026-10-17 (all re-runnable on LVL_Pad39A_Predawn, values in Docs/dimensions.md "Play-through fixes"):
  Tools/Realism/build_weathered.py (M_Weathered master + MI_W_* in /Game/Apollo11/Materials/Weathered; replaces the
  blotchy M_Real look on tower, grating, launcher, elevator, White Room; make_wear_maps.py packs the CC0 masks first),
  build_hatch_area.py (White Room spacecraft end sealed to the BPC, BPC hatch frame/fasteners/open hatch/placards,
  M_BPC_Night; rebuilds the WR shell without the old arc: build_whiteroom.build_shell(front_arc=False)),
  build_pad_stair_fix.py (caged work light + launcher-side railing), build_cockpit_ingress.py (places ACMIngress).
  After re-running build_whiteroom.py, re-run build_hatch_area.build() and build_weathered.build("assign").
- C++ ACMIngress (Apollo11/CMIngress): "Press E to enter" at the hatch, scripted ingress into the left couch, clamped
  seated look, E to exit; drives the camera through UBodycamComponent::SetViewOverride (bodycam look stays on). Path keys
  are a property on the placed CM_Ingress actor (set by build_cockpit_ingress.py). Both it and the elevator bind E
  with bConsumeInput = false (a consumed binding starves the other actor).
- Play-test jobs: "cockpit" / "cockpit:on" (enter, seat checks, look clamp, exit, blocker; :on adds screenshots).
  Custom views: run_playtest_game.ps1 -Shots '[["name",[x,y,z],[pitch,yaw]],...]' -Prefix "x_"; $env:PLAYTEST_SETTLE
  (seconds before each shot, default 5): use ~9 s, the bodycam's auto exposure takes ~10 s to recover after a bright
  view (dark shots right after the White Room are an exposure artifact, not lighting).
- Material Python in 5.8: Custom node extra outputs work (CustomOutput, additional_outputs) and texture objects can be
  Custom inputs (sample with Texture2DSample(Tex, TexSampler, uv)). DistanceToNearestSurface's input pin is
  "World Position"; ObjectLocalBounds outputs are "Half Extents", "Extents", "Min", "Max". Fab/Quixel needs the
  launcher sign-in; CC0 ambientCG sets can be downloaded directly (SourceAssets/Textures/ambientCG/_dl_<id>/).
- Interactive CM panel (2026-10-17): C++ ACMPanel (Apollo11/CMPanel) + UCMControl components (Apollo11/CMControls: toggle,
  rotary, pushbutton, breaker, light, talkback, gauge) built at BeginPlay from Content/Apollo11/Data/CM_Controls.json. Pipeline:
  probe_mdc.py dump() (scan triangles -> Saved/MDC) -> mdc_align.py (true-scale panel renders + depth) -> mdc_place.py (writes the
  JSON; sources in Docs/dimensions.md "CM controls") -> make_cm_textures.py (label/legend atlases, dials, sounds; offline Pillow)
  -> build_cm_controls.build(import|materials|meshes|scan|place). Re-run mdc_place.py + make_cm_textures.py after editing
  controls. Play-test job "panel" / "panel:on" (operates >= 10 of each type via aim_at_control + input_action, logic checks,
  exit/re-enter persistence, seated fps, close-ups). Seat left yaw limit is 120 (panels 7/8); picking prefers the control
  nearest the view ray (the first box hit is often the neighbour's at these slant angles).
- Detail pass (evening 2026-10-17), all re-runnable on LVL_Pad39A_Predawn, values in Docs/dimensions.md "Detail pass":
  Tools/Realism/build_tower_detail.py (tower piping/equipment/placards, instanced), build_arm_detail.py (arms 1-9, White Room
  exterior), build_crane_hook.py, build_cm_closeout.py (hatch closing, windows), build_cockpit_launch.py, build_les.py (stages
  body, les, bpc, sm, sla, sivb), build_ground_detail.py (terrain, roads, buildings, lights). Order rules: after
  build_saturn_realism.build("rocket") re-run build_les.build("body") (it replaces the NASA model's 20-sided body with a smooth
  revolve); after build_saturn_realism.build("place") re-run build_les.build("les") (hides the old truss); after
  build_predawn.build("sky"/"ground") re-run build_ground_detail.build() (it trims Land, hides Beach and the rectangular roads,
  moves Ocean to the 690 m waterline). Shared helpers: Tools/Realism/detail_lib.py.
- Several build calls in one headless session: Tools/run_cmdlet.ps1 -Script Tools/run_steps.py -Call "steps('<file>.json')" with
  a JSON list of [script, call] pairs (nested quotes don't survive -Call). Custom screenshots: run_playtest_game.ps1 -ShotsFile
  views.json (entries [name, [x,y,z], [pitch,yaw], fov]). In the Bash tool, heredocs containing quotes or backticks often break:
  write scripts to a file first.
- Walking (AMyProject1Character): 140 cm/s walk, Shift 300 cm/s outside the cockpit (polled with IsInputKeyDown: the Enhanced
  Input component deletes BindKey), 110 cm/s on stairs / grating, stride 40 + 0.22 v cm shared with the bodycam bob, footsteps from
  /Game/Apollo11/Audio/Footsteps (Tools/Audio/make_footsteps.py + build_footsteps.py). Property names avoid the Horror variant's
  WalkSpeed (UHT forbids shadowing). Play-test jobs: "speed" (walk/brisk/braking/cadence) and "seats[:on]" (hatch closes, all three
  couches, a control operated from each, exit); route timeout is 1500 s at the new speeds.
- Grating: M_Weathered_Grating's mask is box-filtered coverage + temporal dither (TSR resolves it); TSR cvars in DefaultEngine.ini.
  M_Weathered has a "Diamond" parameter (tread plate, used on MI_W_WRDeck).
- Instanced meshes: a material's World->Local transform is in component space, not per instance; use LocalPosition for per-instance
  frames (the placard atlas picks the face from LocalPosition.x). Re-importing a texture right before a play-test run leaves it
  streaming low mips in the screenshots (build_tower_detail imports T_PlacardAtlas only when the PNG is newer than its stamp).
- GeometryScript in 5.8 Python: get_all_vertex_positions(dm, False) / get_all_triangle_indices(dm, False) return (mesh, list, bool);
  append_mesh is on GeometryScript_MeshEdits; detail_lib Mesh.cyl(cap=False) + Mesh.flip() make open nozzle bells.
- Spacecraft fix (2026-10-17 night): Tools/Realism/spacecraft_geom.py is the single source for the stack stations (Boeing Saturn V
  Apollo flight configuration drawing), the boost-cover profile, the CSM radius, the hatch opening and hinge; build_les.py,
  build_saturn_realism.py, build_hatch_area.py, build_cm_closeout.py, build_arm_detail.py and make_paint_maps.py exec it. The NASA
  model's S-IVB/SLA/SM breaks were wrong; the body above the S-IVB aft skirt is built from the stations. Rebuild order after a
  geometry change: build_saturn_realism.build("cover") -> build_cm_closeout.build("bpcwindow") -> build_les.build("body"/"bpc"/"sm"/
  "sla"/"sivb") -> build_saturn_realism.build("night_paint") (after make_paint_maps.py ... _N) -> build_arm_detail.build("arms") ->
  build_hatch_area.build() -> build_whiteroom.rebuild_props() -> build_cm_closeout.build("hatch") -> build_weathered.build_assign().
  build_hatch_area no longer makes the hatch; build_cm_closeout "hatch" does (parts modelled closed, origin on the hinge axis,
  ACMIngress.HatchHingeAxis, labels attached to CM_Hatch_InnerFace). Never call build_whiteroom.build() (it rebuilds the old arc).
- Ground stages added: build_ground_detail.build("ramp") (crawler ramp + crawlerway, MI_Real_Gravel) and build("nightlights")
  (spacecraft searchlights, road poles, spill). GeometryScript append_simple_extrude_polygon needs counter-clockwise outlines
  (clockwise gives negative volume and broken booleans). Detail_lib Mesh methods take tuples, not unreal.Vector.
- Organize content under /Game/Apollo11/ (Blueprints, Materials, Meshes, Maps, Audio).
- Work in small, verifiable steps; take a viewport screenshot after each step to check it.
- Ask before deleting assets or making large irreversible changes.
- The plugin's PCG support is disabled (bHasPCG = false in McpAutomationBridge.Build.cs)
  because of a UE 5.8 delay-load link error. Don't re-enable it.
