#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/ai-module.h"
#include "ns3/queue-disc.h"

#include "rl-aqm-env.h"    // Kendi AQM ortam dosyanız

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("RLAqmSimulation");

// Simülasyon parametreleri
static const uint32_t nSources = 50;      // Kaynak düğüm sayısı
static const uint32_t nSinks = 50;        // Hedef düğüm sayısı
static const double bottleneckBandwidth = 10.0;  // Darboğaz bağlantısı bant genişliği (Mbps)
static const double accessBandwidth = 1000.0;    // Erişim bağlantısı bant genişliği (Mbps)
static const std::string dataRate = "1Gbps";   // Veri hızı
static const uint32_t mtuBytes = 1500;          // MTU boyutu (bayt)
static const double simulationTime = 120.0;     // Simülasyon süresi (saniye)
static const double flowStartTime = 0.1;      // Akış başlangıç ​​zamanı (saniye)

class RLAqmQueueDisc : public QueueDisc
{
public:
    static TypeId GetTypeId (void);
    RLAqmQueueDisc ();
    ~RLAqmQueueDisc () override;

    // Kuyruk disiplini metodları
    bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    Ptr<QueueDiscItem> DoDequeue(void) override;
    Ptr<const QueueDiscItem> DoPeek(void) const ; // const eklendi ve dönüş türü değiştirildi
    bool CheckConfig (void) override;
    void InitializeParams (void) override;
    bool DoDrop(Ptr<QueueDiscItem> item) ;

    // ns3-ai entegrasyon metodları
    virtual void GetState (RLAqmEnv* env);
    virtual void SetAction (uint32_t action);

private:
    Ptr<RLAqmEnv> m_env;
};

// RLAqmQueueDisc metodlarını tanımlayın
TypeId 
RLAqmQueueDisc::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::RLAqmQueueDisc")
        .SetParent<QueueDisc> ()
        .SetGroupName ("TrafficControl")
        .AddConstructor<RLAqmQueueDisc> ();
    return tid;
}

RLAqmQueueDisc::RLAqmQueueDisc ()
{
    NS_LOG_FUNCTION (this);
    m_env = CreateObject<RLAqmEnv> ();
}

RLAqmQueueDisc::~RLAqmQueueDisc ()
{
    NS_LOG_FUNCTION (this);
}

bool RLAqmQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION (this << item);
    return true; // Öğeyi kuyruğa aldık
}

Ptr<QueueDiscItem>
RLAqmQueueDisc::DoDequeue(void)
{
    NS_LOG_FUNCTION(this);
    Ptr<QueueDiscItem> item = GetInternalQueue(0)->Dequeue();
    if (item)
    {
        return item; // Doğru dönüş
    }
    return nullptr; // Paket yoksa nullptr döndür
}

Ptr<const QueueDiscItem>
RLAqmQueueDisc::DoPeek(void) const
{
    NS_LOG_FUNCTION(this);
    Ptr<const QueueDiscItem> item = GetInternalQueue(0)->Peek();
    return item;
}

bool RLAqmQueueDisc::DoDrop(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);
    // Düşürme mantığı
    return true;
}

void RLAqmQueueDisc::GetState(RLAqmEnv* env)
{
    NS_LOG_FUNCTION(this << env);
    m_env = env;
}

void RLAqmQueueDisc::SetAction(uint32_t action)
{
    NS_LOG_FUNCTION(this << action);
    // Eylemi uygulama mantığını buraya ekleyin
}

int main (int argc, char *argv[])
{
    // Komut satırı argümanlarını işleyin
    CommandLine cmd;
    cmd.Parse (argc, argv);

    // Ağ topolojisi oluşturma
    NodeContainer left, right;
    left.Create(nSources);
    right.Create(nSinks);

    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue(dataRate));
    accessLink.SetChannelAttribute("Delay", StringValue("20ms"));

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue(std::to_string(bottleneckBandwidth) + "Mbps"));
    bottleneckLink.SetChannelAttribute("Delay", StringValue("50ms"));

    PointToPointDumbbellHelper dumbbell(nSources, accessLink, nSinks, accessLink, bottleneckLink);

    // Ağa bağlı cihazları oluşturma
    NetDeviceContainer sourceDevices = accessLink.Install(dumbbell.GetLeft());
    NetDeviceContainer sinkDevices = accessLink.Install(dumbbell.GetRight());

    // Bottleneck cihazları kur
    Ptr<Node> leftRouter = dumbbell.GetLeft();
    Ptr<Node> rightRouter = dumbbell.GetRight();
    NetDeviceContainer bottleneckDevices = bottleneckLink.Install(leftRouter, rightRouter);


    // IP adresleme
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer sourceInterfaces, sinkInterfaces, bottleneckInterfaces;
    ipv4.Assign (sourceDevices);
    ipv4.Assign (sinkDevices);
    ipv4.Assign (bottleneckDevices);

    // Trafik modeli oluşturma
    ApplicationContainer sourceApps, sinkApps;
    for (uint32_t i = 0; i < nSources; ++i)
    {
        BulkSendHelper ftp ("ns3::TcpSocketFactory",
                            InetSocketAddress (dumbbell.GetRightIpv4Address(i), 9));
        ftp.SetAttribute ("MaxBytes", UintegerValue (1000000));
        sourceApps.Add (ftp.Install (dumbbell.GetLeft(i)));
    }

    for (uint32_t i = 0; i < nSinks; ++i)
    {
        PacketSinkHelper sink ("ns3::TcpSocketFactory",
                              InetSocketAddress (Ipv4Address::GetAny(), 9));
        sinkApps.Add (sink.Install (dumbbell.GetRight(i)));
    }

    // AQM (RLAqmQueueDisc) kurulumunu yapma
    TrafficControlHelper tcHelper;
    tcHelper.SetRootQueueDisc("ns3::RLAqmQueueDisc");
    tcHelper.Install(bottleneckDevices);

    // FlowMonitor kurulumu
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.Install(dumbbell.GetLeft());
    Ptr<FlowMonitor> monitor2 = flowmon.Install(dumbbell.GetRight());

    // Simülasyonu çalıştırma
    Simulator::Stop (Seconds (simulationTime));
    Simulator::Run ();

    // Simülasyon sonuçlarını analiz edin
    monitor->CheckForLostPackets();

    return 0;
}
