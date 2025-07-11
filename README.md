
# QoS Impact of EDCA Configuration in Wi-Fi 6 (IEEE 802.11ax)

![ns-3 3.43](https://img.shields.io/badge/simulator-ns--3%203.43-blue)
![C++ 17](https://img.shields.io/badge/C%2B%2B-17-blue)
![Python 3.11](https://img.shields.io/badge/Python-3.11-yellow)
![Wi-Fi 6](https://img.shields.io/badge/Wi--Fi-6%20(802.11ax)-orange)

> **Goal –** Quantify how manual tuning of *Enhanced Distributed Channel Access (EDCA)*  
> affects Quality-of-Service for voice / video / best-effort traffic in dense Wi-Fi 6 networks.  
> *Team coursework project for the Teleinformatics (ICT) programme.*

---

## Table of Contents
1. [Project Highlights](#project-highlights)  
2. [Prerequisites](#prerequisites)  
3. [Quick Start](#quick-start)  
4. [Simulation Scenarios](#simulation-scenarios)  
5. [Results](#results)  
6. [Conclusions](#conclusions)  

---

## Project Highlights
- **ns-3 simulation in C++ 17** with the full IEEE 802.11ax PHY/MAC stack.  
- **Python automation** sweeps parameters, seeds multiple runs and exports stats as JSON/CSV.  
- **Metrics analysed:** throughput, latency, jitter, packet-loss.  
- *Optimised* EDCA cuts VoIP median latency by **> 80 %** and slashes packet-loss below **0.1 %** with 10 background stations.  
- Reproducible, single-command workflow – CI-ready.

---

## Prerequisites

| Tool            | Tested Version | Notes                             |
|-----------------|---------------:|-----------------------------------|
| **ns-3**        | 3.43 release   | built via integrated CMake tool   |
| **Python**      | ≥ 3.10 (3.11)  | `matplotlib`, `pandas`, `numpy`   |
| **CMake + gcc** | C++ 17 stack   | to compile ns-3                   |

---

## Quick Start

```bash
# 1) clone ns-3 3.43
git clone https://gitlab.com/nsnam/ns-3-dev.git ns-3.43
cd ns-3.43 && git checkout 3.43

# 2) configure & build (CMake backend)
./ns3 configure
./ns3 build

# 3) run an example scenario (optimal EDCA, 10 background STAs, 120-s sim)
./ns3 run "scratch/project_code.cc \
  --enableEdca=[true|false]           # enable or disable EDCA queue configuration (QoS) \
  --enableBackground=[true|false]     # add background stations with Best Effort traffic \
  --nBgStations=<uint>                # number of background stations (works if enableBackground=true) \
  --simTime=<float>                   # total simulation time in seconds (e.g. 120.0) \
  --warmUpTime=<float>                # initial warm-up phase without traffic, seconds (e.g. 1.0) \
  --output=<string>                   # output filename (JSON) \
  --maxPackets=<uint>                 # maximum packets sent by each client \
  --packetSizeVoip=<uint>             # VoIP packet size in bytes (e.g. 160) \
  --packetSizeVideo=<uint>            # Video packet size in bytes (e.g. 1200) \
  --packetSizeBe=<uint>               # Best Effort / Background packet size (e.g. 1024) \
  --intervalVoipMs=<float>            # VoIP inter-packet interval in ms (e.g. 20.0) \
  --intervalVideoMs=<float>           # Video inter-packet interval in ms (e.g. 33.0) \
  --intervalBeMs=<float>              # BE / Background inter-packet interval in ms (e.g. 50.0) \
  --RngRun=<uint>"                    # RNG seed for repeatability
```

> **Tip:** to execute **all** predefined scenarios and generate plots, simply run  
> ```bash
> python scripts/project_script.py
> ```

---

## Simulation Scenarios

| Scenario             | Background STAs | EDCA settings                              |
|----------------------|-----------------|-------------------------------------------|
| `disable_edca`       | 0 / 3 / 10       | EDCA off – all ACs share identical DCF    |
| `equal_edca`         | 0 / 3 / 10       | Same CW / AIFS / TXOP for VO + VI + BE    |
| `not_optimal_edca`   | 0 / 3 / 10       | Mis-prioritised (BE favoured)             |
| **`optimal_edca` ★** | 0 / 3 / 10       | Priorities: VO > VI > BE (per 802.11ax)   |

**Traffic generators**  
• VoIP – 320 B / 20 ms • Video – 1 880 B / 15 ms • Best-Effort – 1 500 B / 2 ms  

---

## Results

### Testbed Topology  
<p align="center">
  <img src="assets/Topology.png" alt="Topology diagram" width="80%">
</p>

### Key Metrics Dashboards

| Background STAs | Preview |
|-----------------|---------|
| <img src="https://img.shields.io/badge/STAs-0-lightgrey" alt="0 STAs badge">  | ![Metrics 0 BG](assets/Edca_metrics_background_0.png) |
| <img src="https://img.shields.io/badge/STAs-3-lightgrey" alt="3 STAs badge">  | ![Metrics 3 BG](assets/Edca_metrics_background_3.png) |
| <img src="https://img.shields.io/badge/STAs-10-lightgrey" alt="10 STAs badge"> | ![Metrics 10 BG](assets/Edca_metrics_background_10.png) |

---

## Conclusions
- **EDCA is critical:** correct queue prioritisation drastically improves real-time traffic performance.  
- **Latency focus:** with `optimal_edca`, VoIP delay stays below 20 ms even under heavy load (10 STAs).  
- **Mis-configuration costs:** `not_optimal_edca` inflates jitter up to 6× and packet-loss beyond 10 %.  
- **Practical takeaway:** tuning Contention Windows and TXOP per Access Category is a low-cost, high-impact lever for QoS in Wi-Fi 6 deployments.
