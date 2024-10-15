#include "rl-aqm-env.h"

#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RLAqmEnv");

RLAqmEnv::RLAqmEnv()
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
        .AddConstructor<RLAqmEnv> ()
    ;
    return tid;
}

// Eylem uzayı: 5 ayrık eylem
Ptr<OpenGymSpace>
RLAqmEnv::GetActionSpace()
{
    NS_LOG_FUNCTION (this);
    return CreateObject<OpenGymDiscreteSpace> (5);
}

// Gözlem uzayı: 3 sürekli değişkenli kutu uzayı
Ptr<OpenGymSpace>
RLAqmEnv::GetObservationSpace()
{
    NS_LOG_FUNCTION (this);
    uint32_t parameterNum = 3;
    float low = 0.0;
    float high = 1.0; // Normalleştirilmiş değerler
    std::vector<uint32_t> shape = { parameterNum, };
    std::string dtype = TypeNameGet<float>();

    return CreateObject<OpenGymBoxSpace> (low, high, shape, dtype);
}

// Oyun sonu koşulu: Şimdilik yok
bool
RLAqmEnv::GetGameOver()
{
    NS_LOG_FUNCTION (this);
    return false;
}

// Gözlemleri al: Durum değişkenlerini döndür
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

// Ödülü hesapla
float
RLAqmEnv::GetReward()
{
    NS_LOG_FUNCTION (this);
    return m_reward;
}

// Ekstra bilgi: Şimdilik yok
std::string
RLAqmEnv::GetExtraInfo()
{
    NS_LOG_FUNCTION (this);
    return "";
}

// Eylemleri uygula: Bırakma olasılığını güncelle
bool
RLAqmEnv::ExecuteActions(Ptr<OpenGymDataContainer> action)
{
    NS_LOG_FUNCTION (this << action);
    Ptr<OpenGymDiscreteContainer> discreteAction = DynamicCast<OpenGymDiscreteContainer> (action);
    uint32_t actionValue = discreteAction->GetValue();
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

// AQM'den durum bilgisi al
void
RLAqmEnv::SetState(double queueDelay, double linkUtilization, double dropProbability)
{
    NS_LOG_FUNCTION (this << queueDelay << linkUtilization << dropProbability);
    m_queueDelay = queueDelay;
    m_linkUtilization = linkUtilization;
    m_dropProbability = dropProbability;
    // Ödülü hesapla
    m_reward = (pow(linkUtilization, 2) - 0.5) + (2 / (1 + queueDelay / 5) - 1.5);
}

} // namespace ns3