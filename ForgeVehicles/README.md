# Forge Vehicles

Forge Vehicles is Impact-Forge's unified vehicle framework for Unreal Engine 5.8. It consolidates four
previously separate plugins — plus two new ones — into a single modular plugin where *every* vehicle,
from a pickup truck to an FPV kamikaze quad, derives from the same base class and plugs its
propulsion into the same interface.

📖 **Full documentation:** <https://impact-forge.github.io/docs-site/forge-vehicles/overview>
— a page per vehicle family with worked build examples, and a
[complete maths reference](https://impact-forge.github.io/docs-site/forge-vehicles/math)
covering every force, torque and control law the plugin evaluates.

| Source plugin            | Now the module            | Role                                          |
| ------------------------ | ------------------------- | --------------------------------------------- |
| ArcVehicles              | `ForgeVehiclesCore`       | Seat / exit / ignition foundation (the base)  |
| RTune                    | `ForgeVehiclesGround`     | Ground / wheeled / tracked physics            |
| K2 Aircraft Physics      | `ForgeVehiclesRotaryWing` | Helicopter / rotary-wing physics              |
| K2 FixedWing Physics     | `ForgeVehiclesFixedWing`  | Fixed-wing & tiltrotor / VTOL physics         |
| *(new)*                  | `ForgeVehiclesWaterCraft` | Buoyancy-driven boats / water craft           |
| *(new)*                  | `ForgeVehiclesDrones`     | Multirotors, UAVs, loitering munitions        |

Every vehicle type "implements Arc vehicles at the base": each concrete vehicle derives from the
Arc-derived `AForgeVehicle`, so it gains seat configuration, exit points and a replicated engine
ignition component for free, and it plugs its own propulsion into the shared base through a single
interface.

## Modules

* **ForgeVehiclesCore** *(Runtime)* — the essential foundation. Contains the renamed Arc Vehicles
  classes (`AForgeBaseVehicle`, `UForgeVehicleSeatConfig`, `UForgeVehicleExitPoint`,
  `UForgeVehicleEngineSubsystem`, player seat/state components, turret movement), plus the Forge
  foundations:
  * `UForgeEngineIgnitionComponent` + `EForgeEngineIgnitionState` — replicated Off → Igniting → On /
    On → Cutoff → Off ignition state machine with cooldown and release.
  * `FForgeSeatData` / `UForgeSeatConfig` — per-seat input context, held-item visibility, seated
    animation layer, camera and view-restriction data.
  * `IForgeVehicleMovementInterface` — the contract every propulsion solution implements.
  * `AForgeVehicle` — concrete, game-agnostic base carrying a mesh, exit point, ignition component,
    ability system and Enhanced-Input bindings that it routes to the movement interface.
  * `UForgeVehicleLightComponent` + `UForgeVehicleLightAbility` — replicated light fixtures
    addressable by function (headlight, brake, indicator, navigation strobe, …).
  * `UForgeVehicleRunOverComponent` — speed-scaled run-over damage to pawns.
  * `UForgeVehicleAttributeSet` — Health / Fuel / StowedAmmunition on the vehicle's own ASC.
  * `AForgeVehicleGunner` + `UForgeVehicleTurretMovementComp` — possessable gunner seats and turrets.
  * `FForgePIDController` — shared PID used by autopilots and stability loops.
* **AsyncTickPhysics** *(Runtime)* — the async physics-thread ticking support used by the ground
  module. `AAsyncTickPawn` has been re-parented onto `AForgeVehicle` so RTune ground vehicles inherit
  the Arc foundation while keeping their fixed-timestep physics callback.
* **ForgeVehiclesGround** *(Runtime)* — `AForgeGroundVehicle` (formerly `ARTuneVehicle`) and the RTune
  suspension / wheel / articulation / interior components, plus a spline-following PID AI component.
* **ForgeVehiclesRotaryWing** *(Runtime)* — `AForgeRotaryWingVehicle` (formerly `AK2_Aircraft`) and
  its rotor component.
* **ForgeVehiclesFixedWing** *(Runtime)* — `AForgeFixedWingVehicle` (formerly `AK2_FixedWing`) with
  the aero-engine and control-surface components; supports propeller/jet/rotor powerplants,
  multi-engine and asymmetric layouts, differential thrust, thrust vectoring, altitude density,
  transonic drag and tiltrotor / VTOL transitions.
* **ForgeVehiclesWaterCraft** *(Runtime)* — newly authored `AForgeWaterCraft` + multi-pontoon
  `UForgeBuoyancyComponent`, built to the same force-based, tunable design as the other modules.
* **ForgeVehiclesDrones** *(Runtime)* — `AForgeMultirotorVehicle` and four archetypes
  (`AForgeReconQuad`, `AForgeFPVKamikazeQuad`, `AForgeFixedWingUAV`, `AForgeLoiteringMunition`), plus
  battery, control link, jammers, wind, autopilot, gimbal, store release, warhead and operator
  components. The numeric core (`ForgeDrone`) is UObject-free and unit-tested. See
  [`Source/ForgeVehiclesDrones/README.md`](Source/ForgeVehiclesDrones/README.md) for the deep dive.
* **ForgeVehiclesEditor** *(Editor)* — editor-only module carried over from ArcVehicles.

## How the pieces connect

```
AForgeVehiclePawn                         (APawn)
    └── AForgeBaseVehicle                  seat framework, exit points, seat-change events
            └── AForgeVehicle              mesh + ignition + input + GAS + lights + inventory
                ├── AAsyncTickPawn ── AForgeGroundVehicle      (RTune)
                ├── AForgeRotaryWingVehicle                     (K2 helicopter)
                ├── AForgeFixedWingVehicle                      (K2 fixed-wing / VTOL)
                │        └── AForgeFixedWingUAV ── AForgeLoiteringMunition
                ├── AForgeWaterCraft                            (buoyancy boat)
                └── AForgeMultirotorVehicle                     (drone quad)
                         ├── AForgeReconQuad
                         └── AForgeFPVKamikazeQuad
```

* `AForgeVehicle` binds to `UForgeEngineIgnitionComponent::OnEngineIgnitionStateChanged`. When the
  engine reaches `On` it calls `StartEngine()` on the vehicle's movement interface; when it returns
  to `Off` it calls `StopEngine()`.
* `AForgeVehicle`'s Enhanced-Input handlers forward throttle/steering/vertical demand to the movement
  interface. Each vehicle type maps those onto its native controls (RTune throttle/steer, helicopter
  cyclic/yaw/collective, fixed-wing throttle/aileron/elevator, boat thrust/rudder, multirotor sticks).

## Gameplay integrations (on the Core vehicle)

The Core `AForgeVehicle` base — and therefore every vehicle type — ships with:

* **Gunner seats** — `AForgeVehicleGunner` (Core `Seats/`), a Forge-formatted port of the BurningLands
  `ABLVehicleGunner`. It is a possessable seat pawn that drives its own camera from the control
  rotation and feeds Enhanced-Input mouse look into controller yaw/pitch (pair with the turret
  movement component to aim a mounted weapon).
* **Run-over collision** — `UForgeVehicleRunOverComponent` (Core `Components/`) auto-attached to every
  vehicle. It watches the vehicle's collision primitive for hits/overlaps with pawns, and above
  `MinRunOverSpeed` applies (speed-scaled) point damage on the authority and broadcasts
  `OnRunOverActor`. Other vehicles are ignored; the target class filter defaults to `APawn`.
* **Vehicle lights** — `UForgeVehicleLightComponent` fixtures addressed by `EForgeVehicleLightType`,
  replicated so every client sees them, with `UForgeVehicleLightAbility` covering every fixture type
  by configuration alone (including explicit-state drive, for brake lights).
* **Ability system** — the vehicle owns its own ASC and `UForgeVehicleAttributeSet`, so condition,
  fuel and stowed ammunition survive crew changes and work on unmanned platforms with no occupant
  at all. Grant per-vehicle abilities through `DefaultAbilities`.
* **Twisted Bytes interaction** — `AForgeVehicle` implements `ITBIA_Interactable`, so a player can walk
  up and interact to board: the default `OnPostInteract` seats the interacting actor's player in the
  first open seat (fire `OnVehicleInteracted` in Blueprint to customise). Requires the
  TwistedBytes Interaction System plugin.
* **Arc Inventory** — every vehicle carries a `UArcInventoryComponent` (`VehicleInventory`) for cargo /
  mounted equipment. Requires the Arc Inventory plugin.

Because the interaction and inventory integrations live on the Core base, `ForgeVehiclesCore` now
depends on the TwistedBytes Interaction System, Arc Inventory and the GAS stack (GameplayAbilities /
GASCompanion). Those plugins must be present in the project for Forge Vehicles to build.

## Notes / follow-ups for the integrating project

* The plugin targets UE 5.8 (Chaos only). The original ArcVehicles PhysX simulation-filter-shader was
  removed; per-pair collision suppression in `UForgeVehicleEngineSubsystem` now uses the Chaos
  disabled-collisions API.
* Existing content that referenced the old C++ class names (`AArcBaseVehicle`, `ARTuneVehicle`,
  `AK2_Aircraft`, `AK2_FixedWing`) must be repointed at the new `Forge*` names; Blueprints reparent to
  the renamed native classes.
* Each vehicle type keeps its own input setup. Use **either** the shared `AForgeVehicle` input actions
  **or** the physics module's native input, not both, to avoid double-driving a control.
* Ground, rotary-wing and fixed-wing vehicles default to `ReplicationMethod = None` — set it
  explicitly for multiplayer. Multirotors default to `Full`.
* Water-craft networking is intentionally simple (authority/local-driver applies forces; ignition
  replicates the running state); extend as needed for your netcode.
* Set a **mass override** on every vehicle's simulating primitive. Auto-computed mass from a collision
  volume is almost never the intended value, and every force in the plugin is balanced against mass.
* Per-module damage (engine, transmission, tracks, crew, ammo racks) is modelled by the sibling
  **ForgeArmor** plugin, which bridges aggregate structural loss into the `Health` attribute. It is
  optional and defaults entirely off.

## Tests

```
Automation RunTests Forge.Drones     mixer, battery curves and endurance, link factors, relay frames
```

Credits: Arc Vehicles by Puny Human & Garrett Fleenor; RTune and K2 Aircraft physics by Kallisto;
Interaction System by twistedbytes.net; Arc Inventory by Puny Human; consolidation, Core foundations,
water craft and the drone module by Impact-Forge.
