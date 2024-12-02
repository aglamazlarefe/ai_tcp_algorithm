#include "rl_aqm_env.h"
#include "ns3/log.h"

using namespace std;

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RLAqmEnv");

RLAqmEnv::RLAqmEnv()
 : m_queueDelay(0.0), m_linkUtilization(0.0), m_dropProbability(0.0), m_reward(0.0)
{
    NS_LOG_FUNCTION (this);
    SetOpenGymInterface(OpenGymInterface::Get());
}

RLAqmEnv::~RLAqmEnv()
{
    NS_LOG_FUNCTION (this);
}

TypeId
RLAqmEnv::GetTypeId()
{
    static TypeId tid = TypeId ("ns3::RLAqmEnv")
        .SetParent<OpenGymEnv> ()
        .SetGroupName ("Ns3Ai")
        .AddConstructor<RLAqmEnv> ();
    return tid;
}

// Action space: 5 discrete actions
Ptr<OpenGymSpace>
RLAqmEnv::GetActionSpace()
{
    NS_LOG_FUNCTION (this);
    return CreateObject<OpenGymDiscreteSpace> (5);
}

// Observation space: 3 continuous variables (queueDelay, linkUtilization, dropProbability)
Ptr<OpenGymSpace>
RLAqmEnv::GetObservationSpace()
{
    NS_LOG_FUNCTION (this);
    
    uint32_t parameterNum = 3;
    float low = 0.0;
    float high = 1.0; // Normalized values
    std::vector<uint32_t> shape = { parameterNum, };
    std::string dtype = TypeNameGet<float>();
    std::cout << "Creating observation space ";
    return CreateObject<OpenGymBoxSpace> (low, high, shape, dtype);
}

// Game over condition: Currently none
bool
RLAqmEnv::GetGameOver()
{
    NS_LOG_FUNCTION (this);
    return false;
}

// Get the observations: Return state variables
Ptr <OpenGymDataContainer>
RLAqmEnv::GetObservation()
{
    NS_LOG_FUNCTION (this);
    std::vector<uint32_t> shape = { 3, };
    Ptr<OpenGymBoxContainer<float>> box = CreateObject<OpenGymBoxContainer<float>> (shape);
    box->AddValue(m_queueDelay);
    box->AddValue(m_linkUtilization);
    box->AddValue(m_dropProbability);
    return box;
}

// Calculate the reward
float
RLAqmEnv::GetReward()
{
    NS_LOG_FUNCTION (this);
    return m_reward;
}

// Extra info: Currently none
std::string
RLAqmEnv::GetExtraInfo()
{
    NS_LOG_FUNCTION (this);
    return "";
}

// Execute actions: Update drop probability
bool
RLAqmEnv::ExecuteActions(Ptr<OpenGymDataContainer> action)
{
    NS_LOG_FUNCTION (this << action);
    Ptr<OpenGymDiscreteContainer> discreteAction = DynamicCast<OpenGymDiscreteContainer> (action);
    uint32_t actionValue = discreteAction->GetValue();
    NS_LOG_INFO("Received action from Python: " << actionValue); // Log added

    switch (actionValue)
    {
        case 0:
            m_dropProbability = std::max(0.0, std::min(1.0, m_dropProbability + 0.01));
            break;
        case 1:
            m_dropProbability = std::max(0.0, std::min(1.0, m_dropProbability - 0.01));
            break;
        case 2:
            m_dropProbability = std::max(0.0, std::min(1.0, m_dropProbability + 0.1));
            break;
        case 3:
            m_dropProbability = std::max(0.0, std::min(1.0, m_dropProbability - 0.1));
            break;
        default:
            NS_LOG_ERROR ("Invalid action value");
            return false;
    }
    return true;
}

// Get state information from AQM
void
RLAqmEnv::SetState(double queueDelay, double linkUtilization, double dropProbability)
{
    NS_LOG_FUNCTION (this << queueDelay << linkUtilization << dropProbability);
    NS_LOG_INFO("Providing observation to Python."); // Log added
    m_queueDelay = queueDelay;
    m_linkUtilization = linkUtilization;
    m_dropProbability = dropProbability;
    // Calculate reward
    m_reward = (pow(linkUtilization, 2) - 0.5) + (2 / (1 + queueDelay / 5) - 1.5);
}

} // namespace ns3
