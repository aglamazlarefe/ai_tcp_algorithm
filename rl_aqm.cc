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

    #include "rl_aqm_env.h"    // Kendi AQM ortam dosyanız

    using namespace ns3;
    using namespace std;

    NS_LOG_COMPONENT_DEFINE("RLAqmSimulation");


    // Simulation Parameters
    static const uint32_t nSources = 50;      // Number of source nodes
    static const uint32_t nSinks = 50;        // Number of sink nodes
    static const std::string accessBandwidth = "1Gbps";
    static const std::string bottleneckBandwidth = "10Mbps";
    static const std::string accessDelay = "2ms";
    static const std::string bottleneckDelay = "10ms";
    static const double simulationTime =50.0; // Simulation time in seconds



    class RLAqmQueueDisc : public QueueDisc
    {
    public:
        static TypeId GetTypeId (void);
        RLAqmQueueDisc ();
        ~RLAqmQueueDisc () override;

        // Kuyruk disiplini metodları
        bool DoEnqueue(Ptr<QueueDiscItem> item) override;
        Ptr<QueueDiscItem> DoDequeue(void)  override;
        Ptr<const QueueDiscItem> DoPeek(void) const; // const eklendi ve dönüş türü değiştirildi
        bool CheckConfig (void) override;
        void InitializeParams (void) override;
        bool DoDrop(Ptr<QueueDiscItem> item) ;
        void RLAqmQueueDisc::SimulatorCallback();
        double RLAqmQueueDisc::CalculateQueueDelay();
        double RLAqmQueueDisc::CalculateLinkUtilization();

        // ns3-ai entegrasyon metodları
        virtual void GetState (RLAqmEnv* env);
        virtual void SetAction (uint32_t action);

    private:
        Ptr<RLAqmEnv> m_env;
        double m_dropProbability;
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

    bool RLAqmQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item) {
        NS_LOG_FUNCTION(this << item);
        bool succeeded = GetInternalQueue(0)->Enqueue(item);
        if (succeeded) {
            // Update state on enqueue
            double queueDelay = CalculateQueueDelay(); 
            double linkUtilization = CalculateLinkUtilization(); 
            m_env->SetState(queueDelay, linkUtilization, m_dropProbability);
        }
        return succeeded;
    }

    NS_OBJECT_ENSURE_REGISTERED(RLAqmQueueDisc);

    Ptr<QueueDiscItem> RLAqmQueueDisc::DoDequeue() {
        NS_LOG_FUNCTION(this);
        Ptr<QueueDiscItem> item = GetInternalQueue(0)->Dequeue();
        if (item) {
            // Update state on dequeue
            double queueDelay = CalculateQueueDelay();
            double linkUtilization = CalculateLinkUtilization();
            m_env->SetState(queueDelay, linkUtilization, m_dropProbability);
        }
        return item;
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

    bool RLAqmQueueDisc::CheckConfig (void)
    {
        NS_LOG_FUNCTION (this);
        // Yapılandırma kontrolü için gerekli işlemleri burada yapabilirsiniz.
        return true;
    }

    void RLAqmQueueDisc::InitializeParams() {
    NS_LOG_FUNCTION(this);
     if (GetNInternalQueues() == 0) {
         // Add a default DropTail queue
         AddInternalQueue(CreateObject<DropTailQueue<QueueDiscItem>>());
         NS_LOG_INFO("Internal queue initialized for RLAqmQueueDisc.");
     }
    }


    double RLAqmQueueDisc::CalculateQueueDelay() {
        NS_LOG_FUNCTION(this);

        // Get the current queue size in bytes
        uint32_t currentQueueSize = GetInternalQueue(0)->GetNBytes();

        // Retrieve the link bandwidth (e.g., bottleneck link bandwidth in bits per second)
        DataRateValue dataRateValue;
        GetAttribute("DataRate", dataRateValue);
        DataRate linkBandwidth = dataRateValue.Get();

        if (linkBandwidth.GetBitRate() == 0) {
            NS_LOG_WARN("Link bandwidth is zero, queue delay cannot be calculated.");
            return 0.0;
        }

        // Calculate queue delay in seconds
        double queueDelay = static_cast<double>(currentQueueSize) * 8 / linkBandwidth.GetBitRate();
        return queueDelay;
    }


    double RLAqmQueueDisc::CalculateLinkUtilization() {
        NS_LOG_FUNCTION(this);

        // Retrieve the link bandwidth (e.g., bottleneck link bandwidth in bits per second)
        DataRateValue dataRateValue;
        GetAttribute("DataRate", dataRateValue);
        DataRate linkBandwidth = dataRateValue.Get();

        if (linkBandwidth.GetBitRate() == 0) {
            NS_LOG_WARN("Link bandwidth is zero, utilization cannot be calculated.");
            return 0.0;
        }

        // Get the transmitted bytes since the last observation
        uint64_t transmittedBytes = GetInternalQueue(0)->GetTotalEnqueuedBytes();

        // Calculate utilization (0-1)
        double utilization = static_cast<double>(transmittedBytes * 8) / linkBandwidth.GetBitRate();
        return utilization;
    }







    void RLAqmQueueDisc::SimulatorCallback() {
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
        Simulator::Schedule(MilliSeconds(20), &RLAqmQueueDisc::SimulatorCallback, this);
    }

int main(int argc, char* argv[]) {
    // Log Level
        
        LogComponentEnable("RLAqmEnv", LOG_LEVEL_ALL);


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
        for (uint32_t i = 0; i < nSources; ++i) {
            NetDeviceContainer link = accessLink.Install(leftNodes.Get(i), routers.Get(0));
            leftDevices.Add(link.Get(0));
            leftRouterDevices.Add(link.Get(1));
        }

        // Connecting sinks to the right router
        NetDeviceContainer rightDevices, rightRouterDevices;
        for (uint32_t i = 0; i < nSinks; ++i) {
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
        for (uint32_t i = 0; i < nSources; ++i) {
            BulkSendHelper ftp("ns3::TcpSocketFactory",
                InetSocketAddress(rightNodes.Get(i)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9));
            ftp.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited transfer
            sourceApps.Add(ftp.Install(leftNodes.Get(i)));
        }

        for (uint32_t i = 0; i < nSinks; ++i) {
            PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
            sinkApps.Add(sink.Install(rightNodes.Get(i)));
        }
        /*
        sourceApps.Start(Seconds(1.0));
        sourceApps.Stop(Seconds(simulationTime));

        sinkApps.Start(Seconds(1.0));
        sinkApps.Stop(Seconds(simulationTime));
        */
        

        // AQM (RLAqmQueueDisc) kurulumunu yapma
        NS_LOG_INFO("Setting up AQM...");
        TrafficControlHelper tcHelper;
        tcHelper.Uninstall(bottleneckDevices);
        tcHelper.SetRootQueueDisc("ns3::RLAqmQueueDisc");
        std::cout << "aqm kuruldu\n";

        tcHelper.Install(bottleneckDevices);
        
        // FlowMonitor kurulumu
        FlowMonitorHelper flowmon;
        Ptr<FlowMonitor> monitor = flowmon.InstallAll();
        std::cout << "flow monitor kuruldu\n ";

        // Simülasyonu çalıştırma
        Simulator::Schedule(Seconds(0.02), &RLAqmQueueDisc::SimulatorCallback, bottleneckDevices.Get(0));



        NS_LOG_INFO("Starting simulation...");
        Simulator::Stop(Seconds(simulationTime));
        Simulator::Run(); 
        NS_LOG_INFO("Simulation finished.");
        std::cout << "run simulation\n";

        // Simülasyon sonuçlarını analiz edin
        monitor->CheckForLostPackets();
        monitor->SerializeToXmlFile("flow-monitor.xml", true, true);



        Simulator::Destroy();
        std::cout << "destroy simulation\n";


        return 0;
    }