# Transit Performance Analytics (TPA) – NYC Subway

## 1. What TPA Is

This is **TPA**, a tool for analyzing **actual subway movement in New York City**. Unlike typical transit apps that simply display scheduled arrivals and delays reported by the MTA, this system makes **no assumptions** about the accuracy or honesty of those figures. It works by **analyzing the raw telemetry** and inferring actual train movement.

The project is written in **C++20** and structured as a **multi-threaded daemon**. It fetches raw binary protobufs from the MTA’s **GTFS-Realtime** endpoints over HTTPS. Every 30 seconds, it concurrently hits eight different endpoints (such as **A/C/E**, **1/2/3**, **N/Q/R**, etc.).

## 1.2. How TPA Treats the MTA Feed

The MTA’s real-time feed is **stateless**. Each snapshot is a moment in time, with no awareness of what came before. TPA imposes structure onto that by **keeping a running history**. It merges `VehiclePosition` and `TripUpdate` messages into unified train snapshots and stores these in a **SQLite database**, in-memory but backed on disk. Every hour, a cleanup job trims old data beyond a **seven-day window**.

## 1.3. Core Metrics Derived From History

There are two key metrics extracted from this normalized history.

### 1.3.1 Physical Dwell Time

**Physical Dwell Time** is the **actual time a train spends at a specific station**, determined by examining timestamps for a given train ID at a given stop ID. If a train appears to be sitting at Utica Avenue from T=0 to T=600, TPA takes that at face value: it has been stuck there for ten minutes.

This does **not** rely on the MTA’s `status` field or any declared delays; it just looks at **where trains are and how long they stay there**. Known yard and terminal stations are filtered out deliberately to avoid misinterpretation and to keep the focus on real passenger-impacting holds.

### 1.3.2 Lateness

**Lateness** is derived by **comparing the current timestamp to the scheduled arrival time** from the static GTFS schedule. That schedule is stored locally in a large dataset parsed from `stop_times.txt`. It matches realtime trip IDs to static trip IDs, exposing mismatches between the MTA’s reported delay (often zero) and the **actual deviation from the schedule**.

For example, the MTA may claim a train is “on time,” while TPA shows that, relative to the schedule, it is actually **14 minutes late**. TPA always trusts the observed motion and the schedule, not the advertised delay.

## 1.4. Recording and Replaying Sessions

TPA can also **record** the raw realtime feed to a binary file and **replay** it later, driving the **same pipeline and dashboard** as if it were live.

You **do not need an MTA API key** to see the system in action. If you just want to try it without configuring anything, a **pre-recorded session** has been provided. You can replay this data using the `--replay` command:

`--replay recordings/session.rec`

This runs the full pipeline, parsing GTFS-Realtime data, populating the database, and serving the dashboard exactly as if it were processing live data from the MTA.

If you do have an API key and want to gather your own data, you can also **record a session**:

`--record recordings/my_session.rec`

Later, you can replay that same recording offline using the same `--replay` option and inspect that period in as much detail as you want.
