import os
import json
from datetime import datetime

# === NS-3 EXECUTION SETTINGS ===

# Path to ns-3 wrapper command – calls ./ns3 with simulation script
# We pass the entire argument set inside one quoted string (no '--' separator)
ns3_command = "./ns3 run"

# Number of times each scenario will be simulated (statistical significance)
repetitions = 2

# Base value for RngRun parameter (used to vary random seed)
# Each run will increment this to ensure independent simulation results
base_rng = 413743

# Folder to store results and file to aggregate them into
results_dir = "results"
summary_json = "summary-results.json"

# Whether to store JSON output from each individual run (True)
# If False, will only store the final summary JSON
store_individual_jsons = True

# Ensure output directory exists before running
os.makedirs(results_dir, exist_ok=True)

# === FIXED SIMULATION PARAMETERS ===
# These are shared across all test scenarios. They define realistic, high-quality traffic.

fixed_params = {
    "enableBackground": "true",        # Simulate contention from background (BE) stations
    "nBgStations": "3",                # Number of BE background stations
    "simTime": "60.0",                 # Total simulation time in seconds
    "warmUpTime": "20.0",              # Stabilization period with no traffic at start
    "maxPackets": "500000",            # Max number of packets per application (large enough)
    "packetSizeVoip": "320",           # HD Voice (Opus ~128kbps, 20ms frame = 320 bytes)
    "packetSizeVideo": "1880",         # Full HD video encoded chunk (1880B every 15ms ~10Mbps)
    "packetSizeBe": "1500",            # MTU-sized best effort (e.g. file transfer)
    "intervalVoipMs": "20.0",
    "intervalVideoMs": "15.0",
    "intervalBeMs": "2.0"
}

# === SCENARIO DEFINITIONS ===
# These reflect different EDCA configurations:
# - disable_edca: EDCA completely turned off
# - equal_edca: All Access Categories (ACs) use identical contention settings (no priority)
# - not_optimal_edca: Misconfigured priorities – VoIP punished, BE favored
# - optimal_edca: Correct prioritization – VoIP > Video > BE

scenarios = {
    "disable_edca": {
        "enableEdca": "false"
    },
    "equal_edca": {
        "enableEdca": "true",
        "voMinCw": 15, "voMaxCw": 1023, "voAifsn": 3, "voTxop": 0,
        "viMinCw": 15, "viMaxCw": 1023, "viAifsn": 3, "viTxop": 0,
        "beMinCw": 15, "beMaxCw": 1023, "beAifsn": 3, "beTxop": 0
    },
    "not_optimal_edca": {
        "enableEdca": "true",
        "voMinCw": 15, "voMaxCw": 31,  "voAifsn": 5, "voTxop": 1000,
        "viMinCw": 7,  "viMaxCw": 15,  "viAifsn": 3, "viTxop": 0,
        "beMinCw": 7,  "beMaxCw": 31,  "beAifsn": 2, "beTxop": 0
    },
    "optimal_edca": {
        "enableEdca": "true",
        "voMinCw": 3,  "voMaxCw": 7,   "voAifsn": 2, "voTxop": 3008,
        "viMinCw": 7,  "viMaxCw": 15,  "viAifsn": 2, "viTxop": 6016,
        "beMinCw": 15, "beMaxCw": 1023, "beAifsn": 3, "beTxop": 0
    }
}

# === SIMULATION EXECUTION ===

summary_data = []     # List of all JSON entries for final export
run_id = base_rng     # Starting value for RngRun

# For each scenario:
for scenario_name, scenario_params in scenarios.items():
    print(f"\n=== Running scenario: {scenario_name} ===")

    for trial in range(1, repetitions + 1):
        run_id += 1
        tag = f"{scenario_name}_run{trial}"
        output_file = f"{results_dir}/{tag}.json"

        # Merge base config, fixed config, and scenario config into one param set
        full_params = {
            "RngRun": str(run_id),
            "output": output_file if store_individual_jsons else summary_json,
            **fixed_params,
            **scenario_params
        }

        # Compose command string passed to ns-3
        # Format: ./ns3 run "scratch/qos_project2.cc --param1=value --param2=value ..."
        param_string = " ".join(f"--{k}={v}" for k, v in full_params.items())
        cmd = f'{ns3_command} "scratch/qos_project2.cc {param_string}"'

        print(f"[{datetime.now().strftime('%H:%M:%S')}] Running: {tag}")
        os.system(cmd)

        # If individual results stored, collect and merge into summary
        if store_individual_jsons and os.path.exists(output_file):
            try:
                with open(output_file) as f:
                    results = json.load(f)
                    for entry in results:
                        entry["scenario"] = scenario_name
                        entry["runId"] = run_id
                        summary_data.append(entry)
            except Exception as e:
                print(f"⚠️ Failed to load {output_file}: {e}")

# === FINAL SUMMARY SAVE ===

# If skipping individual JSONs, try loading the only output file instead
if not store_individual_jsons and os.path.exists(summary_json):
    with open(summary_json) as f:
        summary_data = json.load(f)

# Save all merged data to a single final JSON
with open(os.path.join(results_dir, summary_json), "w") as f:
    json.dump(summary_data, f, indent=2)

print(f"\n✅ All simulations complete.")
print(f"📄 Combined results saved to: {results_dir}/{summary_json}")
