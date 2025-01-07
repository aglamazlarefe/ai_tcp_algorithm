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
    static const double simulationTime =5.0; // Simulation time in seconds



    class RLAqmQueueDisc : public QueueDisc
    {
    public:
        static TypeId GetTypeId(void);
        RLAqmQueueDisc();
        ~RLAqmQueueDisc() override;

        // QueueDisc methods
        bool DoEnqueue(Ptr<QueueDiscItem> item) override;
        Ptr<QueueDiscItem> DoDequeue(void) override;
        Ptr<const QueueDiscItem> DoPeek(void) const; // const added
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


    // RLAqmQueueDisc metodlarını tanımlayın
    TypeId 
    RLAqmQueueDisc::GetTypeId (void)
    {
        static TypeId tid = TypeId ("ns3::RLAqmQueueDisc")
            .SetParent<QueueDisc> ()
            .SetGroupName ("TrafficControl")
            .AddConstructor<RLAqmQueueDisc>()
            .AddAttribute("MaxSize", 
                          "The maximum number of packets accepted by this queue disc.",
                          QueueSizeValue(QueueSize("100p")),
                          MakeQueueSizeAccessor(&RLAqmQueueDisc::SetMaxSize,
                                                &RLAqmQueueDisc::GetMaxSize),
                          MakeQueueSizeChecker());
        return tid;
     }
    RLAqmQueueDisc::RLAqmQueueDisc() : m_dropProbability(0.0) {
        NS_LOG_FUNCTION(this);
        m_env = CreateObject<RLAqmEnv>();
    }

    RLAqmQueueDisc::~RLAqmQueueDisc ()
    {
        NS_LOG_FUNCTION (this);
    }

    bool RLAqmQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item) {
        NS_LOG_FUNCTION(this << item);
        bool succeeded = GetInternalQueue(0)->Enqueue(item);
        if (succeeded) {
            NS_LOG_INFO("Packet enqueued. Updating state.");
            double queueDelay = CalculateQueueDelay();
            double linkUtilization = CalculateLinkUtilization();
            m_env->SetState(queueDelay, linkUtilization, m_dropProbability);
        } else {
            NS_LOG_ERROR("Failed to enqueue packet.");
        }
        return succeeded;
    }

    NS_OBJECT_ENSURE_REGISTERED(RLAqmQueueDisc);

    Ptr<QueueDiscItem> RLAqmQueueDisc::DoDequeue() {
        NS_LOG_FUNCTION(this);
        Ptr<QueueDiscItem> item = GetInternalQueue(0)->Dequeue();
        if (item) {
            NS_LOG_INFO("Packet dequeued. Updating state.");
            double queueDelay = CalculateQueueDelay();
            double linkUtilization = CalculateLinkUtilization();
            m_env->SetState(queueDelay, linkUtilization, m_dropProbability);
            NS_LOG_INFO("State set: QueueDelay=" << queueDelay << ", LinkUtilization=" << linkUtilization << ", DropProbability=" << m_dropProbability);
            
            
        } else {    
            NS_LOG_WARN("Queue empty. Cannot dequeue.");
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

        bool RLAqmQueueDisc::CheckConfig(void)
    {
        NS_LOG_FUNCTION(this);

        // NetDevice'ı alma
        Ptr<NetDevice> device = GetObject<NetDevice>();
        if (device == nullptr) {
            NS_LOG_WARN("No NetDevice found. Attempting to find one.");

            // Node üzerinden NetDevice arama
            Ptr<Node> node = GetObject<Node>();
            if (node) {
                for (uint32_t i = 0; i < node->GetNDevices(); ++i) {
                    device = node->GetDevice(i);
                    if (device) {
                        NS_LOG_INFO("Found NetDevice through Node");
                        // NetDevice'ı bu QueueDisc'e ekle
                        AggregateObject(device);
                        break;
                    }
                }
            }
        }

        if (GetNInternalQueues() == 0) {
            AddInternalQueue(CreateObject<DropTailQueue<QueueDiscItem>>());
        }

        return true;
    }

        void RLAqmQueueDisc::InitializeParams()
    {
        NS_LOG_FUNCTION(this);

        // NetDevice'ı alma
        Ptr<NetDevice> device = GetObject<NetDevice>();
        if (device == nullptr) {
            NS_LOG_WARN("No NetDevice found during initialization.");

            // Node üzerinden NetDevice arama
            Ptr<Node> node = GetObject<Node>();
            if (node) {
                for (uint32_t i = 0; i < node->GetNDevices(); ++i) {
                    device = node->GetDevice(i);
                    if (device) {
                        NS_LOG_INFO("Found NetDevice through Node during initialization");
                        // NetDevice'ı bu QueueDisc'e ekle
                        AggregateObject(device);
                        break;
                    }
                }
            }
        }

        if (GetNInternalQueues() == 0) {
            AddInternalQueue(CreateObject<DropTailQueue<QueueDiscItem>>());
        }
    }

        double CalculateQueueDelay(Ptr<QueueDisc> queueDisc) {
        // Kuyruk boyutu bilgisini al
        uint32_t currentQueueSize = queueDisc->GetNPackets();
    
        // NetDeviceQueueInterface aracılığıyla NetDevice'e erişim
        Ptr<NetDeviceQueueInterface> netDeviceQueueInterface = queueDisc->GetNetDeviceQueueInterface();
        NS_ASSERT_MSG(netDeviceQueueInterface, "NetDeviceQueueInterface bulunamadı!");
    
        // NetDevice'i al
        Ptr<NetDevice> netDevice = netDeviceQueueInterface->GetObject<NetDevice>();
        NS_ASSERT_MSG(netDevice, "NetDevice bulunamadı!");
    
        // Veri hızı için başlangıç değeri
        double linkDataRate = 0.0;
    
        // Eğer cihaz PointToPointNetDevice ise, kanaldan veri hızını al
        Ptr<PointToPointNetDevice> p2pDevice = netDevice->GetObject<PointToPointNetDevice>();
        if (p2pDevice) {
            Ptr<PointToPointChannel> channel = p2pDevice->GetChannel()->GetObject<PointToPointChannel>();
            NS_ASSERT_MSG(channel, "PointToPointChannel bulunamadı!");
            linkDataRate = channel->GetDataRate().GetBitRate();  // Kanal veri hızını al
        } else {
            // Diğer NetDevice türleri için tahmini bir veri hızı hesapla
            linkDataRate = EstimateAdaptiveDataRate(netDevice);
        }
    
        // Veri hızı doğrulama
        NS_ASSERT_MSG(linkDataRate > 0.0, "Bağlantı hızı bilinmiyor veya sıfır!");
    
        // Kuyruk gecikmesini hesapla (saniye cinsinden)
        double queueDelay = (static_cast<double>(currentQueueSize) * 8) / linkDataRate;
    
        // Milisaniye cinsinden geri döndür
        return queueDelay * 1000;  
    }
    
    // Adaptif hız tahmini fonksiyonu
    double EstimateAdaptiveDataRate(Ptr<NetDevice> device) {
        // WiFi veya LTE gibi adaptif bağlantılar için varsayılan bir hız
        return 54000000.0; // Örnek: 54 Mbps (WiFi varsayılan değeri)
    }   
    double RLAqmQueueDisc::CalculateLinkUtilization() {
        NS_LOG_FUNCTION(this);
    
        // Retrieve the link bandwidth from the associated NetDevice
        Ptr<NetDevice> device = GetObject<NetDevice>();
        if (device == nullptr) {
            NS_LOG_WARN("NetDevice is null, utilization cannot be calculated.");
            return 0.0;  // Handle the case where the NetDevice is not available
        }
    
        // Get the DataRate attribute from the NetDevice
        DataRateValue dataRateValue;
        device->GetAttribute("DataRate", dataRateValue);
        DataRate linkBandwidth = dataRateValue.Get();
    
        if (linkBandwidth.GetBitRate() == 0) {
            NS_LOG_WARN("Link bandwidth is zero, utilization cannot be calculated.");
            return 0.0;
        }
    
        // Get the total number of bytes enqueued in the queue
        uint32_t totalBytes = GetInternalQueue(0)->GetNBytes(); // Get the total bytes in the queue
    
        // Calculate utilization (0-1) using the total bytes in the queue and link bandwidth
        double utilization = static_cast<double>(totalBytes * 8) / linkBandwidth.GetBitRate();  // Convert bytes to bits
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
        NS_LOG_INFO("State set: QueueDelay=" << queueDelay << ", LinkUtilization=" << linkUtilization << ", DropProbability=" << m_dropProbability);


        // Notify OpenGym about the current state
        OpenGymInterface::Get()->NotifyCurrentState();

        // Schedule the next callback at 20 ms
        Simulator::Schedule(MilliSeconds(20), &RLAqmQueueDisc::SimulatorCallback, this); // Note the "this"
    }


int main(int argc, char* argv[]) {
    // Log Level
        
        LogComponentEnable("RLAqmSimulation", LOG_LEVEL_ALL);

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
        
        sourceApps.Start(Seconds(1.0));
        sourceApps.Stop(Seconds(simulationTime));

        sinkApps.Start(Seconds(1.0));
        sinkApps.Stop(Seconds(simulationTime));
        
        

        // AQM (RLAqmQueueDisc) kurulumunu yapma
        NS_LOG_INFO("Setting up AQM...");
        TrafficControlHelper tcHelper;
        tcHelper.Uninstall(bottleneckDevices);
        tcHelper.SetRootQueueDisc("ns3::RLAqmQueueDisc");
        std::cout << "aqm kuruldu\n";

        // QueueDisc'i yükleme ve NetDevice'ı manuel olarak ayarlama
        QueueDiscContainer queueDiscs = tcHelper.Install(bottleneckDevices);
        Ptr<RLAqmQueueDisc> queueDisc = DynamicCast<RLAqmQueueDisc>(queueDiscs.Get(0));

        if (queueDisc) {
            // NetDevice'ı manuel olarak ayarlama
            Ptr<NetDevice> device = bottleneckDevices.Get(0);
            queueDisc->AggregateObject(device);

            // Simülasyonu zamanlama
            Simulator::Schedule(MilliSeconds(20), &RLAqmQueueDisc::SimulatorCallback, queueDisc);
        } else {
            NS_LOG_ERROR("Failed to get RLAqmQueueDisc");
        }

       
        
        // FlowMonitor kurulumu
        FlowMonitorHelper flowmon;
        Ptr<FlowMonitor> monitor = flowmon.InstallAll();
        std::cout << "flow monitor kuruldu\n ";



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