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
#include "ns3/internet-stack-helper.h"
#include "ns3/global-router-interface.h"

#include "rl_aqm_env.h"  // Kendi AQM ortam dosyanız

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("RLAqmSimulation");

// Simulation Parameters
static const uint32_t nSources = 50;  // Number of source nodes
static const uint32_t nSinks = 50;    // Number of sink nodes
static const std::string accessBandwidth = "1Gbps";
static const std::string bottleneckBandwidth = "10Mbps";
static const std::string accessDelay = "2ms";
static const std::string bottleneckDelay = "10ms";
static const double simulationTime = 5.0;  // Simulation time in seconds

class RLAqmQueueDisc : public QueueDisc
{
public:
    static TypeId GetTypeId(void);
    RLAqmQueueDisc();
    ~RLAqmQueueDisc() override;

    // QueueDisc methods
    bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    Ptr<QueueDiscItem> DoDequeue(void) override;
    Ptr<const QueueDiscItem> DoPeek(void) const;  // const added
    bool CheckConfig(void) override;
    void InitializeParams(void) override;
    bool DoDrop(Ptr<QueueDiscItem> item);

    // ns3-ai integration methods
    virtual void GetState(RLAqmEnv* env);
    virtual void SetAction(uint32_t action);

    // Timer callback
    void SimulatorCallback();
    double CalculateQueueDelay();
    double CalculateLinkUtilization();

private:
    Ptr<RLAqmEnv> m_env;
    double m_dropProbability;
};

TypeId RLAqmQueueDisc::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::RLAqmQueueDisc")
        .SetParent<QueueDisc>()
        .SetGroupName("TrafficControl")
        .AddConstructor<RLAqmQueueDisc>()
        .AddAttribute("MaxSize", 
                      "The maximum number of packets accepted by this queue disc.",
                      QueueSizeValue(QueueSize("100p")),
                      MakeQueueSizeAccessor(&RLAqmQueueDisc::SetMaxSize,
                                            &RLAqmQueueDisc::GetMaxSize),
                      MakeQueueSizeChecker());
    return tid;
}

RLAqmQueueDisc::RLAqmQueueDisc()
{
    NS_LOG_FUNCTION(this);
    m_env = CreateObject<RLAqmEnv>();
    
    // NetDevice bağlantısını kontrol etme
    Ptr<NetDevice> device = GetObject<NetDevice>();
    if (device == nullptr) {
        NS_LOG_WARN("No NetDevice associated during construction");
    }
}

RLAqmQueueDisc::~RLAqmQueueDisc()
{
    NS_LOG_FUNCTION(this);
}

bool RLAqmQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);
    bool succeeded = GetInternalQueue(0)->Enqueue(item);
    if (succeeded)
    {
        // Update state on enqueue
        double queueDelay = CalculateQueueDelay();
        double linkUtilization = CalculateLinkUtilization();
        m_env->SetState(queueDelay, linkUtilization, m_dropProbability);
    }
    return succeeded;
}

NS_OBJECT_ENSURE_REGISTERED(RLAqmQueueDisc);

Ptr<QueueDiscItem> RLAqmQueueDisc::DoDequeue()
{
    NS_LOG_FUNCTION(this);
    Ptr<QueueDiscItem> item = GetInternalQueue(0)->Dequeue();
    if (item)
    {
        // Update state on dequeue
        double queueDelay = CalculateQueueDelay();
        double linkUtilization = CalculateLinkUtilization();
        m_env->SetState(queueDelay, linkUtilization, m_dropProbability);
    }
    return item;
}
Ptr<const QueueDiscItem> RLAqmQueueDisc::DoPeek(void) const
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

bool RLAqmQueueDisc::CheckConfig(void)
{
    NS_LOG_FUNCTION(this);
    
    // NetDevice kontrolü ekleyin
    Ptr<NetDevice> device = GetObject<NetDevice>();
    if (device == nullptr) {
        NS_LOG_ERROR("No NetDevice associated with this QueueDisc");
        return false;
    }
    
    return true;
}
void RLAqmQueueDisc::InitializeParams()
{
    NS_LOG_FUNCTION(this);
    if (GetNInternalQueues() == 0)
    {
        // Add a default DropTail queue
        AddInternalQueue(CreateObject<DropTailQueue<QueueDiscItem>>());
        NS_LOG_INFO("Internal queue initialized for RLAqmQueueDisc.");
    }
}

double RLAqmQueueDisc::CalculateQueueDelay()
{
    NS_LOG_FUNCTION(this);

    // Kuyruk boyutunu al (byte cinsinden)
    uint32_t currentQueueSize = GetInternalQueue(0)->GetNBytes();

    // İlişkili NetDevice'i al
    Ptr<NetDevice> device = GetObject<NetDevice>();
    if (device == nullptr)
    {
        NS_LOG_WARN("NetDevice is null, queue delay cannot be calculated.");
        return 0.0; // Eğer NetDevice mevcut değilse, gecikmeyi hesaplayamayız
    }

    // NetDevice'den bağlantı bant genişliğini al
    DataRateValue dataRateValue;
    device->GetAttribute("DataRate", dataRateValue);
    DataRate linkBandwidth = dataRateValue.Get();

    if (linkBandwidth.GetBitRate() == 0)
    {
        NS_LOG_WARN("Link bandwidth is zero, queue delay cannot be calculated.");
        return 0.0; // Bant genişliği sıfır ise, gecikme sıfırdır
    }

    // Kuyruk gecikmesini hesapla (saniye cinsinden)
    double queueDelay = static_cast<double>(currentQueueSize) * 8 / linkBandwidth.GetBitRate();
    return queueDelay; // Hesaplanan gecikmeyi döndür
}

double RLAqmQueueDisc::CalculateLinkUtilization()
{
    NS_LOG_FUNCTION(this);

    // İlişkili NetDevice'i al
    Ptr<NetDevice> device = GetObject<NetDevice>();
    if (device == nullptr)
    {
        NS_LOG_WARN("NetDevice is null, utilization cannot be calculated.");
        return 0.0; // Eğer NetDevice mevcut değilse, kullanım hesaplanamaz
    }

    // NetDevice'den bağlantı bant genişliğini al
    DataRateValue dataRateValue;
    device->GetAttribute("DataRate", dataRateValue);
    DataRate linkBandwidth = dataRateValue.Get();

    if (linkBandwidth.GetBitRate() == 0)
    {
        NS_LOG_WARN("Link bandwidth is zero, utilization cannot be calculated.");
        return 0.0; // Bant genişliği sıfır ise, kullanım sıfırdır
    }

    // Kuyrukta bulunan toplam byte sayısını al
    uint32_t totalBytes = GetInternalQueue(0)->GetNBytes();

    // Kullanımı hesapla (0-1 arasında bir oran)
    double utilization = static_cast<double>(totalBytes * 8) / linkBandwidth.GetBitRate();
    return utilization; // Hesaplanan kullanımı döndür
}

void RLAqmQueueDisc::SimulatorCallback()
{
    NS_LOG_INFO("Simulator callback invoked.");

    // Calculate Queue Delay
    double queueDelay = CalculateQueueDelay(); // Implement this function to compute queue delay.

    // Calculate Link Utilization
    double linkUtilization = CalculateLinkUtilization(); // Implement this function to compute utilization.

    // Update environment state and notify
    m_env->SetState(queueDelay, linkUtilization, m_dropProbability);

    // Notify OpenGym about the current state
    OpenGymInterface::Get()->NotifyCurrentState();

    // Schedule the next callback at 20 ms
    // Simulator::Schedule(MilliSeconds(20), &RLAqmQueueDisc::SimulatorCallback, this); // Note the "this"
}

int main(int argc, char* argv[])
{
    // Log Level
    LogComponentEnable("RLAqmSimulation", LOG_LEVEL_ERROR);

    NS_LOG_INFO("Creating nodes...");
    NodeContainer leftNodes, rightNodes, routers;
    leftNodes.Create(nSources);
    rightNodes.Create(nSinks);
    routers.Create(2); // Two routers for the bottleneck

    NS_LOG_INFO("Creating point-to-point links...");
    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue(accessBandwidth));
    accessLink.SetChannelAttribute("Delay", StringValue(accessDelay));

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue(bottleneckBandwidth));
    bottleneckLink.SetChannelAttribute("Delay", StringValue(bottleneckDelay));

    NS_LOG_INFO("Installing internet stacks...");
    InternetStackHelper stack;
    stack.Install(leftNodes);
    stack.Install(rightNodes);
    stack.Install(routers);

    NS_LOG_INFO("Setting up connections...");
    // Connecting sources to the left router
    NetDeviceContainer leftDevices, leftRouterDevices;
    for (uint32_t i = 0; i < nSources; ++i)
    {
        NetDeviceContainer link = accessLink.Install(leftNodes.Get(i), routers.Get(0));
        leftDevices.Add(link.Get(0));
        leftRouterDevices.Add(link.Get(1));
    }

    // Connecting sinks to the right router
    NetDeviceContainer rightDevices, rightRouterDevices;
    for (uint32_t i = 0; i < nSinks; ++i)
    {
        NetDeviceContainer link = accessLink.Install(rightNodes.Get(i), routers.Get(1));
        rightDevices.Add(link.Get(0));
        rightRouterDevices.Add(link.Get(1));
    }

    // Connecting the two routers (bottleneck link)
    NetDeviceContainer bottleneckDevices = bottleneckLink.Install(routers);

    NS_LOG_INFO("Assigning IP addresses...");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ipv4.Assign(leftDevices);
    ipv4.Assign(leftRouterDevices);

    ipv4.SetBase("10.2.1.0", "255.255.255.0");
    ipv4.Assign(rightDevices);
    ipv4.Assign(rightRouterDevices);

    ipv4.SetBase("10.3.1.0", "255.255.255.0");
    ipv4.Assign(bottleneckDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    NS_LOG_INFO("Setting up traffic...");
    ApplicationContainer sourceApps, sinkApps;
    for (uint32_t i = 0; i < nSources; ++i)
    {
        BulkSendHelper ftp("ns3::TcpSocketFactory",
                           InetSocketAddress(rightNodes.Get(i)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9));
        ftp.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited transfer
        sourceApps.Add(ftp.Install(leftNodes.Get(i)));
    }

    for (uint32_t i = 0; i < nSinks; ++i)
    {
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
        sinkApps.Add(sink.Install(rightNodes.Get(i)));
    }

    // Setup AQM (RLAqmQueueDisc)
    NS_LOG_INFO("Setting up AQM...");
    TrafficControlHelper tcHelper;
    tcHelper.Uninstall(bottleneckDevices);  

    // NetDevice'ların her birine doğrudan QueueDisc uygulayın
    for (uint32_t i = 0; i < bottleneckDevices.GetN(); ++i) {
        Ptr<NetDevice> device = bottleneckDevices.Get(i);

        // QueueDisc'i doğrudan cihaza kurun
        TrafficControlHelper deviceTcHelper;
        deviceTcHelper.SetRootQueueDisc("ns3::RLAqmQueueDisc");
        deviceTcHelper.Install(device);
    }
    


    // FlowMonitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    std::cout << "Flow monitor setup completed." << std::endl;

    // Run the simulation
    NS_LOG_INFO("Starting simulation...");
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    NS_LOG_INFO("Simulation finished.");

    // Analyze the results
    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("flow-monitor.xml", true, true);

    Simulator::Destroy();
    std::cout << "Simulation destroyed." << std::endl;

    return 0;
}
