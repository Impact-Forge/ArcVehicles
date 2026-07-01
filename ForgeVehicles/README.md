# Forge Vehicles

Forge Vehicles is Impact-Forge's unified vehicle framework for Unreal Engine 5.8. It consolidates four
previously separate plugins into one modular plugin:

| Source plugin            | Now the module         | Role                                             |
| ------------------------ | ---------------------- | ------------------------------------------------ |
| ArcVehicles              | `ForgeVehiclesCore`    | Seat / exit / ignition foundation (the base)     |
| RTune                    | `ForgeVehiclesGround`  | Ground / wheeled / tracked physics               |
| K2 Aircraft Physics      | `ForgeVehiclesRotaryWing` | Helicopter / rotary-wing physics              |
| K2 FixedWing Physics     | `ForgeVehiclesFixedWing`  | Fixed-wing & tiltrotor / VTOL physics         |
| *(new)*                  | `ForgeVehiclesWaterCraft` | Buoyancy-driven boats / water craft           |

Every vehicle type now "implements Arc vehicles at the base": each concrete vehicle derives from the
Arc-derived `AForgeVehicle`, so it gains seat configuration, exit points and a replicated engine
ignition component for free, and it plugs its own propulsion into the shared base through a single
interface.

## Modules

* **ForgeVehiclesCore** *(Runtime)* — the essential foundation. Contains the renamed Arc Vehicles
  classes (`AForgeBaseVehicle`, `UForgeVehicleSeatConfig`, `UForgeVehicleExitPoint`,
  `UForgeVehicleEngineSubsystem`, player seat/state components, turret movement), plus the new
  foundations distilled from the BurningLands reference implementation:
  * `UForgeEngineIgnitionComponent` + `EForgeEngineIgnitionState` — replicated Off → Igniting → On /
    On → Cutoff → Off ignition state machine with cooldown and release.
  * `FForgeSeatData` / `UForgeSeatConfig` — per-seat input context, held-item visibility, seated
    animation layer, camera and view-restriction data.
  * `IForgeVehicleMovementInterface` — the contract every propulsion solution implements.
  * `AForgeVehicle` — concrete, game-agnostic base carrying a mesh, exit point, ignition component
    and Enhanced-Input throttle/steer/vertical/engine bindings that it routes to the movement
    interface. Every vehicle type below extends this.
* **AsyncTickPhysics** *(Runtime)* — the async physics-thread ticking support used by the ground
  module. `AAsyncTickPawn` has been re-parented onto `AForgeVehicle` so RTune ground vehicles inherit
  the Arc foundation while keeping their fixed-timestep physics callback.
* **ForgeVehiclesGround** *(Runtime)* — `AForgeGroundVehicle` (formerly `ARTuneVehicle`) and the RTune
  suspension / wheel / articulation / interior components.
* **ForgeVehiclesRotaryWing** *(Runtime)* — `AForgeRotaryWingVehicle` (formerly `AK2_Aircraft`) and
  its rotor component.
* **ForgeVehiclesFixedWing** *(Runtime)* — `AForgeFixedWingVehicle` (formerly `AK2_FixedWing`) with
  the aero-engine and control-surface components; supports propeller/jet, multi-engine and
  tiltrotor / VTOL transitions.
* **ForgeVehiclesWaterCraft** *(Runtime)* — newly authored `AForgeWaterCraft` + multi-pontoon
  `UForgeBuoyancyComponent`, built to the same force-based, tunable design as the other modules.
* **ForgeVehiclesEditor** *(Editor)* — editor-only module carried over from ArcVehicles.

## How the pieces connect

```
AForgeVehiclePawn                         (APawn)
    └── AForgeBaseVehicle                  seat framework, exit points, seat-change events
            └── AForgeVehicle              mesh + UForgeEngineIgnitionComponent + input, drives
                │                          IForgeVehicleMovementInterface
                ├── AAsyncTickPawn ── AForgeGroundVehicle      (RTune)
                ├── AForgeRotaryWingVehicle                     (K2 helicopter)
                ├── AForgeFixedWingVehicle                      (K2 fixed-wing / VTOL)
                └── AForgeWaterCraft                            (buoyancy boat)
```

* `AForgeVehicle` binds to `UForgeEngineIgnitionComponent::OnEngineIgnitionStateChanged`. When the
  engine reaches `On` it calls `StartEngine()` on the vehicle's movement interface; when it returns
  to `Off` it calls `StopEngine()`.
* `AForgeVehicle`'s Enhanced-Input handlers forward throttle/steering/vertical demand to the movement
  interface. Each vehicle type maps those onto its native controls (RTune throttle/steer, helicopter
  cyclic/yaw/collective, fixed-wing throttle/aileron/elevator, boat thrust/rudder).

## Notes / follow-ups for the integrating project

* The plugin targets UE 5.8 (Chaos only). The original ArcVehicles PhysX simulation-filter-shader was
  removed; per-pair collision suppression in `UForgeVehicleEngineSubsystem` now uses the Chaos
  disabled-collisions API.
* Existing content that referenced the old C++ class names (`AArcBaseVehicle`, `ARTuneVehicle`,
  `AK2_Aircraft`, `AK2_FixedWing`) must be repointed at the new `Forge*` names; Blueprints reparent to
  the renamed native classes.
* Each vehicle type keeps its own input setup. Use **either** the shared `AForgeVehicle` input actions
  **or** the physics module's native input, not both, to avoid double-driving a control.
* Water-craft networking is intentionally simple (authority/local-driver applies forces; ignition
  replicates the running state); extend as needed for your netcode.

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
* **Twisted Bytes interaction** — `AForgeVehicle` implements `ITBIA_Interactable`, so a player can walk
  up and interact to board: the default `OnPostInteract` seats the interacting actor's player in the
  first open seat (fire `OnVehicleInteracted` in Blueprint to customise). Requires the
  TwistedBytes Interaction System plugin.
* **Arc Inventory** — every vehicle carries a `UArcInventoryComponent` (`VehicleInventory`) for cargo /
  mounted equipment. Requires the Arc Inventory plugin.

Because the interaction and inventory integrations live on the Core base, `ForgeVehiclesCore` now
depends on the TwistedBytes Interaction System, Arc Inventory and the GAS stack (GameplayAbilities /
GASCompanion). Those plugins must be present in the project for Forge Vehicles to build.

Credits: Arc Vehicles by Puny Human & Garrett Fleenor; RTune and K2 Aircraft physics by Kallisto;
Interaction System by twistedbytes.net; Arc Inventory by Puny Human; consolidation, Core foundations
and water craft by Impact-Forge.
