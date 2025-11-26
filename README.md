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

## 1.5. MTA Reported

I wanna talk about how, while building and testing TPA, a consistent discrepancy was observed in the **reported delay values** across different subway lines.(Something that you will see while using the tool often, especially on the dashboard and the .db file)

When inspecting the **L line**, the delay field in the real-time GTFS data is often populated with actual integers, values like `382`, `506`, or `-135`, meaning there are real deviations from schedule. In contrast, almost every other line (such as **A/C/E, 1/2/3, N/Q/R**) reports a delay of exactly **0**, even in cases where the train is clearly running behind schedule. TPA’s own calculation might show a train arriving **15 minutes late**, but the feed insists it’s “on time.”

The thing tho is, this wasn’t due to a parser error on my side. To rule that out, I added logging into the protobuf deserialization path and inspected the `stop_time_update` messages directly. The **L line** data consistently includes a populated delay field. Other lines either **omit the field** (which defaults to `0`) or explicitly include a value of `0`.

I eventually found a plausible explanation for why most other lines don’t provide a meaningful delay value. The **L and 7 lines** use **CBTC (Communications-Based Train Control)**, a modern signaling system where the train’s position and timing are known in real time. The rest of the subway system still runs on **fixed-block signaling**, which offers only coarse-grained location and timing data. So I came to the conclusion that because of this, the MTA feed often lacks the information necessary to calculate delay, and defaults to reporting `0` (**once again, this is a conclusion I have made up; that last sentence is NOT confirmed, but the probabilities are high**).

This is one of the reason TPA calculates its own **lateness metric** rather than relying on the feed’s reported delay values. The official numbers are often missing or wrong. Comparing scheduled arrival times from the **static GTFS files** to real-time timestamps gives a more accurate picture of how delayed a train really is.
