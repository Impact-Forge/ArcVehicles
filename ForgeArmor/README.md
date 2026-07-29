# Forge Armor

Impact-Forge's realistic vehicle armor & damage framework for UE 5.8, in the
War Thunder / Gunner HEAT PC / Squad tradition:

* **Real armor values** — plates authored in real millimetres with material
  RHA-equivalence (vs kinetic and vs chemical energy separately), line-of-sight
  thickness from impact obliquity, per-penetrator-class slope effects, overmatch
  and ricochet bands.
* **Real ammunition data** — every ammo nature carries a penetration datum
  (mm RHA at a reference speed) and a DeMarre velocity falloff per penetrator
  class (AP/APCBC/APCR/APDS/APFSDS/HEAT/HESH/HE, APHE bursting charges).
* **Per-module damage** — engine, transmission, fuel, ammo stowage, turret
  drive, gun, optics, radio, tracks, individual wheels, crew stations; module
  states drive mobility / firepower / catastrophic kill states.
* **Behind-armor effects** — spall cones with fragment counts and energy
  budgets, residual shaped-charge jets, HESH scabs, APHE internal bursts, crew
  casualties with incapacitation/lethality rolls.
* **Fire & cook-off** — flammability per module, fire spread, crew burns, ammo
  cook-off timers, wet stowage and blow-out panels.
* **ERA** — consumable reactive tiles with separate KE/CE cuts (reduced vs
  long rods), replicated so clients can hide spent tiles.

## How it integrates with Terminal Ballistics

Forge Armor **never modifies Terminal Ballistics source** and never re-implements
its exterior ballistics. The contract uses only public, update-stable TB APIs:

1. Armor zones are collision primitives carrying TB's plain `IMPENETRABLE`
   component tag. TB stops any simulated bullet at the plate and reports a full
   `BulletImpactEvent` (server, game thread) with velocity, bullet data and the
   launch payload.
2. The game mode forwards that event to `UForgeArmorImpactRouter::RouteBulletImpact`,
   which runs the armor math and applies interior damage.
3. Perforations and ricochets continue as **fresh TB bullets** re-fired with the
   residual velocity (chain-depth capped), so overpenetrations hit whatever is
   next — including the far side of the same vehicle, other vehicles, or crew
   (crew hits flow through the game's existing soldier damage pipeline).
4. Soldier and environment ballistics are untouched: with the armor system
   disabled (`UForgeArmorSettings::bArmorSystemEnabled = false`, the default)
   every projectile behaves exactly as before the plugin was installed.

The `ForgeArmorTB` module is the only code that includes TB headers. A TB update
that renames the impenetrable tag is caught at module startup (the tag name is
re-read from `TB::Tags`); everything else binds to exported TB types only.

## Modules

| Module | Depends on | Purpose |
|---|---|---|
| `ForgeArmorCore` | engine only | penetration math, data assets, armor/damage components, registry, fire/cook-off, automation tests |
| `ForgeArmorTB` | + TerminalBallistics | impact routing, ammo performance map, continuation re-fires |
| `ForgeArmorVehicles` | + ForgeVehicles (optional) | adapter mapping module damage onto ForgeVehicles pawns (stubs out when ForgeVehicles is absent) |

## Setting up a vehicle

1. Add three components to the vehicle actor:
   * `UForgeVehicleArmorComponent` — assign a **UForgeArmorProfile**
   * `UForgeVehicleDamageComponent` — assign a **UForgeVehicleDamageModel**
   * an adapter: `UForgeVehiclesDamageAdapterComponent` (ForgeVehicles pawns) or
     your game's own `IForgeVehicleDamageAdapter` implementation
2. Author the **armor profile**: one zone per plate (glacis, turret front, hull
   sides, ...) with socket placement, box extent, material tag and thickness in
   mm. Stack applique/ERA over base armor with `BackingZoneId`. Zone boxes only
   need to cover the silhouette — the *authored* thickness drives the math, not
   the box depth.
3. Author the **damage model**: modules with volumes placed on sockets, HP,
   energy capacity, flammability, crew stations with seat indices and role tags.
4. Author (once per project) the **UForgeArmorMaterialSet** (a real-world-anchored
   default catalogue is built in) and the **UForgeAmmoPerformanceMap** with specs
   for your ammunition natures; reference both from Project Settings → Forge Armor.
5. Wire the game mode's TB events to the router (see the consuming project's
   integration guide). Weapons should pass their `UBulletDataAsset` as the TB
   launch `Payload` so armor can resolve ammo specs and re-fire continuations.
6. Enable `bArmorSystemEnabled` in Project Settings → Forge Armor.

Wheels/tires that should stay directly shootable (rubber damage rather than
armor interactions) keep their projectile collision by carrying the component
tag `ForgeArmor.KeepProjectileCollision`; hits on them arrive through TB's
injure event and damage nearby `bExternal` modules.

## Warheads (HEAT / HESH / HE)

Rocket/shell actors evaluate their `UForgeWarheadSpec` against vehicles through
the world registry:

```
UForgeArmorRegistrySubsystem::Get(this)->EvaluateCEImpact(Hit, Warhead, StandoffMM, Instigator, Causer, Result);
UForgeArmorRegistrySubsystem::Get(this)->EvaluateHESHImpact(Hit, Warhead, Instigator, Causer, Result);
UForgeArmorRegistrySubsystem::Get(this)->EvaluateBlast(Origin, Warhead, Instigator, Causer);
```

## Validation

* `Automation RunTests Forge.Armor` — numeric self-tests of the penetration
  math against worked real-world examples (.50 AP vs BTR side plate, 30 mm APDS
  vs composite turret, RPG-7 vs RHA/ERA/glacis arrays). No gameplay required.
* `forge.Armor.Debug 1` — draw penetration solves, outcomes and continuations.
* Data assets validate in-editor (duplicate ids, missing materials, bad
  references) via `IsDataValid`.

## Dark launch

Everything defaults **off**. `bArmorSystemEnabled=false` leaves zones inert
(no impenetrable tags, no collision takeover, routers return false immediately).
Individual systems (crew damage, cook-off, fire, spall...) are tuned/disabled
through `UForgeArmorSettings` and the consuming game's config without code.
