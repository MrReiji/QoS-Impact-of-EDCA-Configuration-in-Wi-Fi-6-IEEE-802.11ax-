#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManualEdcaConfig");

int main(int argc, char *argv[])
{
  // === SIMULATION SETTINGS ===

  bool enableEdca = true;                  // Enable EDCA manual differentiation for QoS queues
  bool enableBackground = false;           // Enable background BE traffic (non-prioritized)
  uint32_t nBgStations = 5;                // Number of background stations generating BE traffic
  double simTime = 120.0;                  // Total simulation time in seconds
  double warmUpTime = 1.0;                 // Time before application traffic starts [s]
  std::string output = "edca-results.json"; // Output JSON filename for FlowMonitor statistics

  // === TRAFFIC PARAMETERS (High-quality settings for all traffic classes) ===

  uint32_t maxPackets = 1000000;           // Large value to ensure traffic runs for entire simTime

  // --- VOICE (AC_VO): HD Voice / Opus codec (~128 kbps)
  uint32_t packetSizeVoip = 320;           // 20 ms frame @ 128 kbps = 320 bytes
  double intervalVoipMs = 20.0;            // 50 packets per second (every 20 ms)

  // --- VIDEO (AC_VI): Full HD 1080p (~10 Mbps)
  uint32_t packetSizeVideo = 1880;         // Large MPEG chunk or encoded frame payload
  double intervalVideoMs = 15.0;           // Every 15 ms → 10 Mbps (1880×8 / 0.015)

  // --- BEST EFFORT (AC_BE): heavy file transfer (~6 Mbps)
  uint32_t packetSizeBe = 1500;            // Full MTU-sized UDP packet
  double intervalBeMs = 2.0;               // Every 2 ms → 6 Mbps (1500×8 / 0.002)

  // === EDCA QoS CONFIGURATION PARAMETERS ===
  // CW: Contention Window (random backoff window)
  // AIFSN: Arbitration Inter Frame Space Number
  // TXOP: Transmission Opportunity duration in microseconds

  uint32_t voMinCw = 3, voMaxCw = 7, voAifsn = 2, voTxop = 3008;     // VO: highest priority
  uint32_t viMinCw = 7, viMaxCw = 15, viAifsn = 2, viTxop = 6016;    // VI: high priority
  uint32_t beMinCw = 15, beMaxCw = 1023, beAifsn = 3, beTxop = 0;    // BE: low priority

  // === COMMAND LINE PARSING ===

  CommandLine cmd;
  cmd.AddValue("enableEdca", "Enable EDCA", enableEdca);
  cmd.AddValue("enableBackground", "Enable background BE traffic", enableBackground);
  cmd.AddValue("nBgStations", "Number of background stations", nBgStations);
  cmd.AddValue("simTime", "Simulation time [s]", simTime);
  cmd.AddValue("warmUpTime", "Warm-up time [s]", warmUpTime);
  cmd.AddValue("output", "Output JSON file", output);

  cmd.AddValue("maxPackets", "Max number of packets", maxPackets);
  cmd.AddValue("packetSizeVoip", "VoIP packet size [bytes]", packetSizeVoip);
  cmd.AddValue("packetSizeVideo", "Video packet size [bytes]", packetSizeVideo);
  cmd.AddValue("packetSizeBe", "BE packet size [bytes]", packetSizeBe);
  cmd.AddValue("intervalVoipMs", "VoIP interval [ms]", intervalVoipMs);
  cmd.AddValue("intervalVideoMs", "Video interval [ms]", intervalVideoMs);
  cmd.AddValue("intervalBeMs", "BE interval [ms]", intervalBeMs);

  // EDCA (Voice, Video, Best Effort)
  cmd.AddValue("voMinCw", "VO Min CW", voMinCw);
  cmd.AddValue("voMaxCw", "VO Max CW", voMaxCw);
  cmd.AddValue("voAifsn", "VO AIFSN", voAifsn);
  cmd.AddValue("voTxop", "VO TXOP [us]", voTxop);

  cmd.AddValue("viMinCw", "VI Min CW", viMinCw);
  cmd.AddValue("viMaxCw", "VI Max CW", viMaxCw);
  cmd.AddValue("viAifsn", "VI AIFSN", viAifsn);
  cmd.AddValue("viTxop", "VI TXOP [us]", viTxop);

  cmd.AddValue("beMinCw", "BE Min CW", beMinCw);
  cmd.AddValue("beMaxCw", "BE Max CW", beMaxCw);
  cmd.AddValue("beAifsn", "BE AIFSN", beAifsn);
  cmd.AddValue("beTxop", "BE TXOP [us]", beTxop);

  cmd.Parse(argc, argv);

  // === NODE CREATION ===

  NodeContainer staNodes; staNodes.Create(3);    // Station clients: VoIP, Video, BE
  NodeContainer apNode; apNode.Create(1);        // Single access point
  NodeContainer bgNodes;
  if (enableBackground) bgNodes.Create(nBgStations);
  NodeContainer allSta = staNodes;
  if (enableBackground) allSta.Add(bgNodes);

  // === PHY/MAC/Wi-Fi CONFIGURATION ===

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy; phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ax); // 802.11ax = Wi-Fi 6
  wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager"); // Rate control

  WifiMacHelper mac;
  Ssid ssid = Ssid("edca-ssid");

  // === INSTALL STATIONS ===
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, allSta);

  // === INSTALL ACCESS POINT ===
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconGeneration", BooleanValue(true));
  NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

  // === MOBILITY: STATIC (nodes do not move) ===
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(allSta);
  mobility.Install(apNode);

  // === INTERNET STACK + IP ADDRESSES ===
  InternetStackHelper internet;
  internet.Install(allSta);
  internet.Install(apNode);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("192.168.1.0", "255.255.255.0");
  auto apIf = ipv4.Assign(apDevices);
  auto staIfs = ipv4.Assign(staDevices);

  // === MANUAL EDCA CONFIGURATION ===
  if (enableEdca)
  {
    // AC_VO – Voice: smallest contention window, short AIFS, short TXOP (~3 ms)
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VO_EdcaTxopN/MinCw", UintegerValue(voMinCw));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VO_EdcaTxopN/MaxCw", UintegerValue(voMaxCw));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VO_EdcaTxopN/Aifsn", UintegerValue(voAifsn));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VO_EdcaTxopN/TxopLimit", TimeValue(MicroSeconds(voTxop)));

    // AC_VI – Video: moderate contention window, short AIFS, longer TXOP (~6 ms)
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VI_EdcaTxopN/MinCw", UintegerValue(viMinCw));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VI_EdcaTxopN/MaxCw", UintegerValue(viMaxCw));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VI_EdcaTxopN/Aifsn", UintegerValue(viAifsn));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VI_EdcaTxopN/TxopLimit", TimeValue(MicroSeconds(viTxop)));

    // AC_BE – Best Effort: large contention window, longer AIFS, no TXOP
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_EdcaTxopN/MinCw", UintegerValue(beMinCw));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_EdcaTxopN/MaxCw", UintegerValue(beMaxCw));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_EdcaTxopN/Aifsn", UintegerValue(beAifsn));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_EdcaTxopN/TxopLimit", TimeValue(MicroSeconds(beTxop)));
  }

  // === INSTALL UDP APPLICATIONS ===

  // VoIP Traffic: port 8000
  UdpServerHelper voipSrv(8000); voipSrv.Install(apNode.Get(0));
  UdpClientHelper voipCl(apIf.GetAddress(0), 8000);
  voipCl.SetAttribute("MaxPackets", UintegerValue(maxPackets));
  voipCl.SetAttribute("Interval", TimeValue(MilliSeconds(intervalVoipMs)));
  voipCl.SetAttribute("PacketSize", UintegerValue(packetSizeVoip));
  voipCl.Install(staNodes.Get(0)).Start(Seconds(warmUpTime));

  // Video Traffic: port 8001
  UdpServerHelper vidSrv(8001); vidSrv.Install(apNode.Get(0));
  UdpClientHelper vidCl(apIf.GetAddress(0), 8001);
  vidCl.SetAttribute("MaxPackets", UintegerValue(maxPackets));
  vidCl.SetAttribute("Interval", TimeValue(MilliSeconds(intervalVideoMs)));
  vidCl.SetAttribute("PacketSize", UintegerValue(packetSizeVideo));
  vidCl.Install(staNodes.Get(1)).Start(Seconds(warmUpTime));

  // Best Effort Traffic: port 8002
  UdpServerHelper beSrv(8002); beSrv.Install(apNode.Get(0));
  UdpClientHelper beCl(apIf.GetAddress(0), 8002);
  beCl.SetAttribute("MaxPackets", UintegerValue(maxPackets));
  beCl.SetAttribute("Interval", TimeValue(MilliSeconds(intervalBeMs)));
  beCl.SetAttribute("PacketSize", UintegerValue(packetSizeBe));
  beCl.Install(staNodes.Get(2)).Start(Seconds(warmUpTime));

  // Optional Background Traffic (one port per station: 9000+)
  if (enableBackground)
  {
    for (uint32_t i = 0; i < nBgStations; ++i)
    {
      UdpServerHelper bgSrv(9000 + i); bgSrv.Install(apNode.Get(0));
      UdpClientHelper bgCl(apIf.GetAddress(0), 9000 + i);
      bgCl.SetAttribute("MaxPackets", UintegerValue(maxPackets));
      bgCl.SetAttribute("Interval", TimeValue(MilliSeconds(intervalBeMs)));
      bgCl.SetAttribute("PacketSize", UintegerValue(packetSizeBe));
      bgCl.Install(bgNodes.Get(i)).Start(Seconds(warmUpTime));
    }
  }

  // === FLOW MONITOR TO MEASURE QoS METRICS ===
  FlowMonitorHelper fm;
  Ptr<FlowMonitor> monitor = fm.InstallAll();

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  // === EXPORT FLOW STATISTICS TO JSON ===
  std::ofstream out(output);
  out << "[\n";
  bool first = true;
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(fm.GetClassifier());

  for (auto &flow : monitor->GetFlowStats())
  {
    if (!first) out << ",\n";
    first = false;

    auto id = flow.first;
    auto &st = flow.second;
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(id);

    std::string flowLabel = "Unknown";
    if (t.destinationPort == 8000) flowLabel = "VoIP";
    else if (t.destinationPort == 8001) flowLabel = "Video";
    else if (t.destinationPort == 8002) flowLabel = "BestEffort";
    else if (t.destinationPort >= 9000 && t.destinationPort < 9000 + nBgStations)
      flowLabel = "Background" + std::to_string(t.destinationPort - 9000 + 1);

    double tp = st.rxBytes * 8.0 / (st.timeLastRxPacket.GetSeconds() - st.timeFirstTxPacket.GetSeconds());

    out << "  {\n"
        << "    \"flowId\": " << id << ",\n"
        << "    \"flowLabel\": \"" << flowLabel << "\",\n"
        << "    \"txPackets\": " << st.txPackets << ",\n"
        << "    \"rxPackets\": " << st.rxPackets << ",\n"
        << "    \"delaySum\": " << st.delaySum.GetSeconds() << ",\n"
        << "    \"jitterSum\": " << st.jitterSum.GetSeconds() << ",\n"
        << "    \"throughput\": " << tp << "\n"
        << "  }";
  }
  out << "\n]\n";
  out.close();

  std::cout << "Simulation finished. Output: " << output << std::endl;
  Simulator::Destroy();
  return 0;
}
