#include "rl_aqm_env.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RLAqmEnv");

RLAqmEnv::RLAqmEnv()
    : m_queueDelay(0.0), m_linkUtilization(0.0), m_dropProbability(0.0), m_reward(0.0) {
    //NS_LOG_FUNCTION("set opengyminterface");
    SetOpenGymInterface(OpenGymInterface::Get());

}

RLAqmEnv::~RLAqmEnv() {
   // NS_LOG_FUNCTION(this);
}

TypeId RLAqmEnv::GetTypeId() {
    static TypeId tid = TypeId("ns3::RLAqmEnv")
                            .SetParent<OpenGymEnv>()
                            .SetGroupName("Ns3Ai")
                            .AddConstructor<RLAqmEnv>();
    return tid;
}

Ptr<OpenGymSpace> RLAqmEnv::GetActionSpace() {
    // NS_LOG_FUNCTION(this);
    // Discrete actions: {do nothing, increase/drop probabilities by 0.01 or 0.1}
    return CreateObject<OpenGymDiscreteSpace>(5);
}

Ptr<OpenGymSpace> RLAqmEnv::GetObservationSpace() {
   // NS_LOG_FUNCTION(this);
    std::cout << "getobservation\n";
    uint32_t parameterNum = 3; // [queueDelay, linkUtilization, dropProbability]
    float low = 0.0;
    float high = 1.0; // Normalized values
    std::vector<uint32_t> shape = {parameterNum};
    std::string dtype = TypeNameGet<float>();
    return CreateObject<OpenGymBoxSpace>(low, high, shape, dtype);
}

bool RLAqmEnv::GetGameOver() {
    NS_LOG_FUNCTION(this);
    return false; // No terminal state for this environment
}

Ptr<OpenGymDataContainer> 
RLAqmEnv::GetObservation() {
    NS_LOG_INFO("GetObservation called.");
    std::vector<uint32_t> shape = {3};  // 3 gözlem değişkeni
    Ptr<OpenGymBoxContainer<float>> box = CreateObject<OpenGymBoxContainer<float>>(shape);
    box->AddValue(m_queueDelay);
    box->AddValue(m_linkUtilization);
    box->AddValue(m_dropProbability);
    NS_LOG_INFO("Observation sent: QueueDelay=" << m_queueDelay 
                 << ", LinkUtilization=" << m_linkUtilization 
                 << ", DropProbability=" << m_dropProbability);
    return box;
}


float RLAqmEnv::GetReward() {
    NS_LOG_INFO("GetReward called. Reward=" << m_reward);
    return m_reward;
}

std::string RLAqmEnv::GetExtraInfo() {
    //NS_LOG_FUNCTION("get extra info");
    return "";
}

bool RLAqmEnv::ExecuteActions(Ptr<OpenGymDataContainer> action) {
    //NS_LOG_INFO("ExecuteActions called.");

    // Aksiyonun geçerliliğini kontrol et
    if (action == nullptr) {
        NS_LOG_ERROR("Received null action!");
        return false;
    }

    Ptr<OpenGymDiscreteContainer> discreteAction = DynamicCast<OpenGymDiscreteContainer>(action);
    if (discreteAction == nullptr) {
        NS_LOG_ERROR("Failed to cast action to OpenGymDiscreteContainer.");
        return false;
    }

    uint32_t actionValue = discreteAction->GetValue();
    NS_LOG_INFO("Python'dan alınan aksiyon: " << actionValue);

    // Geçersiz aksiyon kontrolü
    if (actionValue >= 5) { // 0-4 aralığında olmalı
        NS_LOG_ERROR("Invalid action value: " << actionValue);
        return false;
    }

    // Aksiyon işleme
    double oldDropProbability = m_dropProbability; // Önceki değeri saklayın
    switch (actionValue) {
        case 0: // Do nothing
            NS_LOG_INFO("Action 0: No change to drop probability.");
            break;
        case 1: // Increase drop probability by 0.01
            m_dropProbability = std::min(1.0, m_dropProbability + 0.01);
            NS_LOG_INFO("Action 1: Increased drop probability by 0.01.");
            break;
        case 2: // Decrease drop probability by 0.01
            m_dropProbability = std::max(0.0, m_dropProbability - 0.01);
            NS_LOG_INFO("Action 2: Decreased drop probability by 0.01.");
            break;
        case 3: // Increase drop probability by 0.1
            m_dropProbability = std::min(1.0, m_dropProbability + 0.1);
            NS_LOG_INFO("Action 3: Increased drop probability by 0.1.");
            break;
        case 4: // Decrease drop probability by 0.1
            m_dropProbability = std::max(0.0, m_dropProbability - 0.1);
            NS_LOG_INFO("Action 4: Decreased drop probability by 0.1.");
            break;
        default:
            NS_LOG_ERROR("Unhandled action value: " << actionValue);
            return false;
    }

    // DropProbability güncellemelerini kontrol et
    if (std::isnan(m_dropProbability) || std::isinf(m_dropProbability)) {
        NS_LOG_ERROR("Invalid drop probability value after action: " << m_dropProbability);
        m_dropProbability = oldDropProbability; // Eski değere geri dön
        return false;
    }

    NS_LOG_INFO("Updated drop probability: " << m_dropProbability);

    // Simülasyon durumunu bildirin
    OpenGymInterface::Get()->NotifyCurrentState();

    return true;
}

void RLAqmEnv::SetState(double queueDelay, double linkUtilization, double dropProbability) {
    NS_LOG_FUNCTION(this << queueDelay << linkUtilization << dropProbability);
    m_queueDelay = queueDelay;
    m_linkUtilization = linkUtilization;
    m_dropProbability = dropProbability;

    // Reward function as per the paper
    m_reward = (pow(m_linkUtilization, 2) - 0.5) + (2 / (1 + m_queueDelay / 5) - 1.5);
    if (std::isnan(m_reward) || std::isinf(m_reward)) {
        NS_LOG_ERROR("Invalid reward value: " << m_reward);
        m_reward = 0.0;
    }
    NS_LOG_INFO("Reward calculated: " << m_reward);
}



}  // namespace ns3
