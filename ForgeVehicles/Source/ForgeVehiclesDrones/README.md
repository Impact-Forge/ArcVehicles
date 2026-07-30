# ForgeVehiclesDrones

Unmanned aircraft for Forge Vehicles: multirotors, small fixed-wing UAVs, and the armed versions of
both. Everything here sits on top of the existing plugin rather than beside it — a drone is a
`AForgeVehicle` like any other, so it inherits seats (it just has none), the ignition component, the
GAS layer, and the movement-interface contract, and it is possessed and flown through the same input
path as a truck.

The module adds three things the crewed vehicles never needed:

1. **A multirotor flight model.** The rotary-wing module models a helicopter as a single body with
   decorative rotors; that cannot represent a quadcopter, where attitude *is* the differential
   between four thrust sources. `AForgeMultirotorVehicle` mixes four (or more) motors and applies
   each one's thrust at its own arm, so tilting genuinely produces translation.
2. **The things that make an unmanned aircraft unmanned** — a battery that runs out, a radio link
   with finite range that terrain and jammers can take away, an autopilot to fly the aircraft when
   the link is gone, and wind that pushes a light airframe around.
3. **Payloads** — a stabilised gimbal, a store release, and a warhead.

Fixed-wing drones reuse `ForgeVehiclesFixedWing` unchanged: its aerodynamics are dimensionally sound
SI, so a 2 kg airframe is a retune, not a reimplementation.

## Classes

| Class | Role |
| --- | --- |
| `AForgeMultirotorVehicle` | Multirotor flight model. X-mixer, per-motor `AddForceAtLocation`, Angle and Acro modes, motor-out handling, packed unreliable stick RPC. |
| `AForgeReconQuad` | Camera quadcopter: stable, long-endurance, gimbal + two small stores. |
| `AForgeFPVKamikazeQuad` | FPV strike quad: Acro by default, high thrust-to-weight, short endurance, warhead. |
| `AForgeFixedWingUAV` | Hand- or catapult-launched reconnaissance wing. Adds launch-by-impulse and unattended loiter. |
| `AForgeLoiteringMunition` | The UAV plus a warhead and one irreversible decision (`CommitToTarget`). |
| `UForgeDroneBatteryComponent` | Watt-hour pack, load model, replicated charge, low/critical/depleted events. An empty pack stops the motors. |
| `UForgeDroneLinkComponent` | Control link: range × line-of-sight × (1 − jamming). Degrades input before it reaches the aircraft; runs a failsafe when lost. |
| `UForgeDroneJammerComponent` / `UForgeDroneJammerSubsystem` | EW emitters and the registry links query. |
| `UForgeDroneAutopilotComponent` | `Manual` / `AltitudeHold` / `PositionHold` / `Orbit` / `ReturnToHome` / `TerminalDive`, server-side, driving the four movement-interface axes. |
| `UForgeWindSubsystem` | Settable base wind plus deterministic gusts, sampled per position and time. |
| `UForgeDroneGimbalComponent` | Two-axis world-stabilised camera mount. |
| `UForgeDroneDropReleaseComponent` | Store release that inherits the aircraft's velocity and credits the operator. |
| `UForgeDroneWarheadComponent` | Arm-delay + minimum-distance safety, contact and proximity fuzes, `OnDetonated`. |
| `UForgeDroneOperatorComponent` | Hands a player control of the drone — by possession or by input relay — and puts them back where they were afterwards. |
| `FForgeDroneInputFrame` | One packed, quantised, sequence-numbered frame of relayed stick input. |
| `ForgeDroneMath` | The pure maths: mixer, battery curves, link factors. Unit-tested, no engine state. |

## Archetype tuning

These are the values authored in each archetype's constructor. They are the starting point, not a
constraint — every one is `EditDefaultsOnly` or `EditAnywhere`, so a Blueprint child rescales any of
them without touching C++.

| | FPV kamikaze | Recon quad | Fixed-wing UAV | Loitering munition |
| --- | --- | --- | --- | --- |
| All-up mass | 1.2 kg | 0.92 kg | 2.0 kg | 2.5 kg |
| Propulsion | 4 × 8 N, 120 mm arms | 4 × 5.5 N, 160 mm arms | 1 pusher (Blueprint) | 1 pusher (Blueprint) |
| Thrust-to-weight | 2.7 | 2.4 | — | — |
| Default flight mode | **Acro**, 720/720/400 °/s | **Angle**, 25° tilt limit | — | — |
| Battery | 28 Wh, 8 W avionics, 1200 W max | 60 Wh, 10 W avionics, 450 W max | 90 Wh, 12 W avionics, 200 W max | 50 Wh, 14 W avionics, 240 W max |
| Endurance | ≈ 6 min hovering | ≈ 28 min hovering | ≈ 66 min cruising | ≈ 22 min cruising |
| Link range | 5 km | 9 km | 12 km | 10 km |
| On link loss | `Continue` | `ReturnToHome` | station-keep → orbit | `ReturnToHome`, `Continue` once committed |
| Payload | Warhead (2 s / 30 m) | Gimbal (−90°…30°) + 2 stores | — | Warhead (3 s / 50 m) |
| Runs people over | **yes** | no | no | no |

The endurance figures are what the load model actually produces from the constants in the same row —
`Forge.Drones.Math.Battery` recomputes all four from those numbers, so retuning an archetype and
leaving this table stale fails the test rather than quietly drifting.

A few of these deserve explanation rather than a number:

* **The FPV quad keeps its run-over component.** A five-inch quad arriving at forty metres a second
  is a physical impact whether or not the warhead functions. The other three suppress it, along with
  the vehicle inventory and the occupant exit point — a sub-kilogram airframe has no cargo hold and
  nobody boards it.
* **`Continue` on the FPV quad's link loss is deliberate.** A drone already committed to a run should
  finish it on its last commands rather than turning around. The scouts are worth recovering, so they
  come home instead.
* **Acro versus Angle is the substantive difference between the two quads,** not a preference. In
  Acro the sticks command rotation *rates* with no self-levelling, which is what allows the
  continuous rolls and dives of FPV flying and how these are really flown. In Angle they command an
  attitude within a tilt limit, so releasing the sticks parks the aircraft in a hover — which is what
  you want under a camera.

## Two things live on the Blueprint, not in C++

**Fixed-wing engine thrust.** `UForgeAeroEngineComponent`'s tunables (`MaxThrust` and friends) are
protected on the component, so a vehicle class cannot reach them from its constructor. Add the engine
component in the Blueprint and set:

| | Fixed-wing UAV | Loitering munition |
| --- | --- | --- |
| `MaxThrust` | 12 N | 18 N |
| `ThrustAxis` | X | X |
| `Mode` | Physics and Animation | Physics and Animation |

The endurance figures above assume a settled cruise throttle of 0.35 for the UAV and 0.5 for the
munition. If you author a different `MaxThrust`, cruise lands at a different throttle and endurance
moves with it: total load is `AvionicsLoadW + MaxPropulsionLoadW × throttle`, and endurance in
minutes is `60 × CapacityWh / load`.

**The store class on the recon quad.** `Stores->StoreClass` is left unset, because what a drone drops
is a project decision.

## The battery has two power curves, and the airframe picks one

A rotor holding an aircraft up and a propeller pulling a wing along are not the same machine, so they
do not share a curve:

* **Rotorcraft** use `Battery::PropulsionLoadW`, where power rises with roughly thrust^1.5. This is
  why a quad hovering gently lasts far longer than the same quad flown hard, and why the FPV quad's
  six minutes is a hover figure it will not see in a real sortie.
* **Wings** use `Battery::CruisePropulsionLoadW`, which is linear in throttle. A wing carries its own
  weight, so the propeller only has to overcome drag; useful power is thrust × airspeed. Borrowing
  the rotor curve here would make a wing look implausibly efficient at every part-throttle setting it
  ever cruises at, and would overstate its endurance several-fold.

`UForgeDroneBatteryComponent::GatherPropulsionLoadW` chooses by owner type. An unrecognised airframe
draws nothing from propulsion and is expected to report its own load through `SetPayloadLoadW`.

When the pack empties, a multirotor gets `SetPowerScale(0)` and falls; a wing has its engine shut
down and glides, because that is what actually happens.

## Link loss, and what "hold position" means to a wing

`UForgeDroneLinkComponent` evaluates quality at 4 Hz as range roll-off × line of sight × (1 −
jamming), where jamming takes the *worse* of the two ends — denying the operator's antenna works as
well as denying the aircraft. Below `DegradedQualityThreshold` the link starts dropping and delaying
stick inputs through `FilterInput`, so a marginal signal feels marginal before it fails. Past
`LostQualityThreshold` for `LinkLossGraceSeconds`, the failsafe engages.

`FailsafeHover` asks the autopilot for `PositionHold`. A wing cannot hold a point — it has to keep
flying to stay up — so `UForgeDroneAutopilotComponent::SetMode` translates that request into an orbit
around the current position for any non-multirotor airframe. This is why the fixed-wing UAV's row
above says "station-keep → orbit", and it applies equally to the hold that `ReturnToHome` settles
into on arrival.

Regaining the link only hands control back if the failsafe was what took it: a pilot-commanded orbit
or dive survives the signal flickering.

## Flying one: the operator, and their body

`UForgeDroneOperatorComponent` is present on every archetype, and there are two ways in:

| | `TakeControl` (possession) | `BeginRelayControl` (relay) |
| --- | --- | --- |
| The operator's pawn | Left standing, **unpossessed** | Stays theirs, **still possessed** |
| The drone | Possessed by their controller | Never possessed; owned by their connection |
| Camera | Follows possession | `SetViewTargetWithBlend` onto the drone |
| Input | The ordinary pawn input path | Packed frames pushed via `SetRelayFlightInput` |
| Cost | Runs the engine's possess/unpossess path on the body | Sidesteps it entirely |

Possession is the simpler route and the camera and input come along for free. The relay exists because
unpossessing a soldier is not free: it tears down their input component, and a project that binds
anything in a one-shot initialiser will not rebind it on the way back. The relay never touches that
path, so the body stays fully under its owner's control the whole time — which also means the soldier
can be made to visibly stand there working a controller.

**Neither is the seat system, deliberately.** A seat attaches its occupant to the vehicle, hides them
and disables their movement. That is right for a driver and wrong for a drone operator: the soldier
stays standing exactly where they were, in the open, visible and shootable. Nothing here moves, hides
or protects them, because that exposure is the price of using a drone. Under possession the parked body
is kept owned by its own controller so it carries on replicating to that client — the operator can
watch themselves being shot at.

The body is also where the radio is: it is what gets passed to the link as the antenna, so range,
terrain occlusion and jamming are all measured from the soldier rather than from the drone. Flying
deep behind a ridge is therefore a decision, not a free move.

### The relay wire format

`SetRelayFlightInput(longitudinal, lateral, yaw, vertical)` and `SetRelayGimbalInput(pitch, yaw)`
accumulate into one `FForgeDroneInputFrame`, flushed to the server at `RelaySendHz` (30 by default).
Call them as often as you like — the send rate is what determines what a drone costs on the wire, not
how often the project pushes input.

* **One unreliable frame, not four reliable RPCs.** Reliability is worthless for a value that will be
  superseded a thirtieth of a second later; a lost packet should cost one frame of staleness, not a
  stalled channel. Axes are quantised to a byte each, which is more precision than a thumbstick has.
* **Frames carry a wrapping sequence number,** because unreliable delivery reorders. Applying an older
  frame after a newer one drags the aircraft back through inputs the pilot has already left behind,
  which reads as a stutter in the controls.
* **Discrete commands go separately and reliably.** `SendRelayCommand(EForgeDroneRelayCommand)` covers
  arming, flight-mode toggle, store release, warhead arming and the autopilot modes. A lost "release
  store" is not corrected by the next frame, so it does not belong in an unreliable one.
* **A stalled stream centres the sticks** after `RelayInputTimeoutSeconds`, handing the aircraft to its
  failsafe rather than leaving it flying its last input forever.
* **An active autopilot wins.** Relayed sticks are not applied while the autopilot is flying, so a
  pilot does not find themselves fighting a return-to-home they never cancelled.

Everything still goes through `Link->FilterInput`, so a marginal signal is felt as sticky, delayed
controls on both routes.

Three things can go wrong, and each has a defined outcome:

| | What happens |
| --- | --- |
| **The drone is destroyed** while being flown | The operator is released in `EndPlay`, before the pawn goes away — possession hands the controller back to its body, a relay restores its view target. Losing a drone must not leave a player staring at nothing. |
| **The operator's body is destroyed** mid-flight | The antenna died with them, so the link goes dead and the aircraft runs its failsafe. By default the operator also loses the drone; under possession that leaves them pawnless, which is the same state any other death produces, for the project's death handling to pick up. Set `bReleaseControlOnOperatorBodyLost = false` to let them keep flying a radio-less aircraft. |
| **Someone else tries to take a drone already being flown** | Refused with `AlreadyControlled`. A stale entry left by a disconnect does not lock the airframe out — only a live controller counts. |

`TakeControl` and `BeginRelayControl` are both server-side and deliberately *not* client RPCs: until
control is taken the drone is not owned by the operator's connection, so a request from that client
would be dropped. Call them from something the player does own — a granted ability, an interaction on a
deployed drone, or the item that carries it. Release is the opposite case and needs no glue: both
routes make the drone connection-owned, so `ServerRequestRelease()` is callable straight from the
client, as are the relay's own input and command calls.

An airframe nobody has ever taken does not run its failsafe, even though it has no link. Without that
latch a recon quad sitting on the ground unclaimed would try to fly itself home.

## Setting up a drone

1. Subclass the archetype in Blueprint. Set the mesh on `Core` and give it a collision primitive
   sized like the airframe — the mass override in `BeginPlay` handles the weight, but the collision
   shape still determines what it hits.
2. For the quads, either leave `Motors` empty (an X-configuration quad is generated from
   `ArmLengthCm` and `DefaultMotorThrustN`) or author each motor's `RelativeLocationCm`, `bClockwise`
   and `MaxThrustN` explicitly. Alternate the propeller directions or the aircraft will spin in
   hover.
3. Assign the input actions, including `RollAction` and `FlightModeToggleAction` which crewed
   vehicles do not use. Per-vehicle contexts go in `DriverMappingContext`; anything shared across
   your drones belongs in `VehicleBaseMappingContext` at a lower priority.
4. For fixed-wing archetypes, add and tune the engine component as above.
5. Call `Launch()` on the wings — they have no undercarriage and no runway. Give the quads `Battery`
   charge and let them lift off.
6. Give the player a way to reach `Operator->TakeControl(...)` or `BeginRelayControl(...)` on the
   server — an ability, an interaction, or a deployment item. For the relay route, also bind a drone
   input context on the operator's own pawn and push the axes into `SetRelayFlightInput`.
7. Wire the warhead's `OnDetonated` to whatever resolves damage in your project. The module
   deliberately carries no damage logic and no dependency on ForgeArmor; in BattleSpace that binding
   lives in `ForgeArmorVehicles`.

## Wind and EW

`UForgeWindSubsystem` is a world subsystem — call `SetWindFromHeading(degrees, metresPerSecond)` and
`SetGusting(amplitude, frequency)` from your game mode, per map. Sampling is deterministic in
position and time, so every machine computes the same gust without replicating anything. The
multirotor's drag term routes through `ComputeWindForce`, which acts on *airspeed* rather than ground
speed — that difference is what makes a light drone visibly crab across a hover.

Jammers register themselves with `UForgeDroneJammerSubsystem` on activation. Give an emitter a
`BandTag` and only links on that band are affected; leave it unset to deny everything.

## Tests

```
Forge.Drones.Math.Mixer     hover symmetry, saturation shift, yaw signs, motor-out
Forge.Drones.Math.Battery   the four archetype endurance figures, both power curves, integration
Forge.Drones.Math.Link      range roll-off, jamming, line of sight, combination bounds
Forge.Drones.Relay.Frame    axis quantisation, clamping, sequence ordering across the wrap
```

Run them from the editor's Session Frontend, or headless:

```
UnrealEditor-Cmd.exe <Project>.uproject -ExecCmds="Automation RunTests Forge.Drones; Quit" -unattended -nullrhi
```
